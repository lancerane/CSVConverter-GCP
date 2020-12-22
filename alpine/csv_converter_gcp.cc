#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "csv_conv2.h"

#include "google/cloud/storage/client.h"

namespace be = boost::beast;
namespace asio = boost::asio;
namespace po = boost::program_options;
using tcp = boost::asio::ip::tcp;

po::variables_map parse_args(int& argc, char* argv[]) {
  // Initialize the default port with the value from the "PORT" environment
  // variable or with 8080.
  auto port = [&]() -> std::uint16_t {
    auto env = std::getenv("PORT");
    if (env == nullptr) return 8080;
    auto value = std::stoi(env);
    if (value < std::numeric_limits<std::uint16_t>::min() ||
        value > std::numeric_limits<std::uint16_t>::max()) {
      std::ostringstream os;
      os << "The PORT environment variable value (" << value
         << ") is out of range.";
      throw std::invalid_argument(std::move(os).str());
    }
    return static_cast<std::uint16_t>(value);
  }();

  // Parse the command-line options.
  po::options_description desc("Server configuration");
  desc.add_options()
      //
      ("help", "produce help message")
      //
      ("address", po::value<std::string>()->default_value("0.0.0.0"),
       "set listening address")
      //
      ("port", po::value<std::uint16_t>()->default_value(port),
       "set listening port");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << "\n";
  }
  return vm;
}


vector<string> ListObjectsWithPrefix(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [list objects with prefix] [START storage_list_files_with_prefix]
  vector<string> fileList;
  namespace gcs = google::cloud::storage;
  [&fileList](gcs::Client client, std::string const& bucket_name,
     std::string const& bucket_prefix) {
    for (auto&& object_metadata :
         client.ListObjects(bucket_name, gcs::Prefix(bucket_prefix))) {
      if (!object_metadata) {
        throw std::runtime_error(object_metadata.status().message());
      }

      if (object_metadata->name().length() > 12) {
        fileList.push_back(object_metadata->name());
      }

      std::cout << "bucket_name=" << object_metadata->bucket()
                << ", object_name=" << object_metadata->name() << object_metadata->name().length() << "\n";

    }
  }
  //! [list objects with prefix] [END storage_list_files_with_prefix]
  (std::move(client), argv.at(0), argv.at(1));

  return fileList;
}



int main(int argc, char* argv[]) try {

  std::string const bucket_name = "edd23232";
  std::string const raw_data_dir = "unprocessed";

  // Create aliases to make the code easier to read.
  namespace gcs = google::cloud::storage;

  auto client = gcs::Client::CreateDefaultClient().value();
  vector<string> fileList = ListObjectsWithPrefix(client, {bucket_name, "unprocessed"});
  printVector(fileList);

  // Create a client to communicate with Google Cloud Storage. This client
  // uses the default configuration for authentication and project id.
  google::cloud::StatusOr<gcs::Client> dl_client =
      gcs::Client::CreateDefaultClient();

  if (!dl_client) {
    std::cerr << "Failed to create Storage Client, status=" << dl_client.status() << "\n";
    return 1;
  }

  boost::filesystem::create_directory(raw_data_dir);

  for(int i=0; i < fileList.size(); i++) {

    std::string base = raw_data_dir;
    std::vector<std::string> results;
    boost::split(results, fileList[i], [](char c){return c == '/';});

    if (results.size() > 2) {
      for (int i(1); i< results.size()-1; i++) {

        base += "/";
        base +=  results[1];
        boost::filesystem::create_directory(base);
      }
    }

    std::string abs_dl_path = "/r/" + fileList[i];
    dl_client->DownloadToFile(bucket_name, fileList[i], abs_dl_path);

    fileList[i] = abs_dl_path;
  }

  printVector(fileList);

  po::variables_map vm = parse_args(argc, argv);

  if (vm.count("help")) return 0;

  auto address = asio::ip::make_address(vm["address"].as<std::string>());
  auto port = vm["port"].as<std::uint16_t>();
  std::cout << "Listening on " << address << ":" << port << std::endl;

  auto handle_session = [&fileList](tcp::socket socket) {
    auto report_error = [](be::error_code ec, char const* what) {
      std::cerr << what << ": " << ec.message() << "\n";
    };

    be::error_code ec;
    for (;;) {
      be::flat_buffer buffer;

      // Read a request
      be::http::request<be::http::string_body> request;
      be::http::read(socket, buffer, request, ec);
      if (ec == be::http::error::end_of_stream) break;
      if (ec) return report_error(ec, "read");

      char delim = ',';
      // if (fileList.size() == 0) return 1; //return error if there were no files to convert, so we know about it

      bool conversionFailure = false;
      for(int i=0; i < fileList.size(); i++){
        // cout << "Converting: " << fileList[i] << endl;
        if (convertFile(fileList[i], delim) != 0){
          cout << fileList[i] << " failed to convert" << endl;
          conversionFailure = true;
        }
      }

      cout << "Conversions complete" << endl << endl;

      // if (conversionFailure) return 1;
      // else return 0;

      // Send the response
      // Respond to any request with a "Hello World" message.
      be::http::response<be::http::string_body> response{be::http::status::ok,
                                                         request.version()};
      response.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
      response.set(be::http::field::content_type, "text/plain");
      response.keep_alive(request.keep_alive());
      std::string greeting = "Hello ";
      auto const* target = std::getenv("TARGET");
      greeting += target == nullptr ? "World" : target;
      greeting += "\n";
      response.body() = std::move(greeting);
      response.prepare_payload();
      be::http::write(socket, response, ec);
      if (ec) return report_error(ec, "write");
    }
    socket.shutdown(tcp::socket::shutdown_send, ec);
  };

  asio::io_context ioc{/*concurrency_hint=*/1};
  tcp::acceptor acceptor{ioc, {address, port}};
  for (;;) {
    auto socket = acceptor.accept(ioc);
    if (!socket.is_open()) break;
    // Run a thread per-session, transferring ownership of the socket
    std::thread{handle_session, std::move(socket)}.detach();
  }






  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception caught " << ex.what() << '\n';
  return 1;
}