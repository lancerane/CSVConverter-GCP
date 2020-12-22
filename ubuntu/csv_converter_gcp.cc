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


// Set up http listening stuff using boost 
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

// Helper function that uses the GCP API and returns a vector of strings where each is the filename of a relevant file in the GCP bucket starting with the prefix given as an arg
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
        fileList.push_back(object_metadata->name());

      // std::cout << "bucket_name=" << object_metadata->bucket()
      //           << ", object_name=" << object_metadata->name() << "\n";

    }
  }
  //! [list objects with prefix] [END storage_list_files_with_prefix]
  (std::move(client), argv.at(0), argv.at(1));

  return fileList;
}

void DownloadFile(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [download file]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::string const& file_name) {
    google::cloud::Status status =
        client.DownloadToFile(bucket_name, object_name, file_name);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "Downloaded " << object_name << " to " << file_name << "\n";
  }
  //! [download file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void UploadFile(google::cloud::storage::Client client,
                std::vector<std::string> const& argv) {
  //! [upload file] [START storage_upload_file]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& file_name,
     std::string const& bucket_name, std::string const& object_name) {
    // Note that the client library automatically computes a hash on the
    // client-side to verify data integrity during transmission.
    StatusOr<gcs::ObjectMetadata> metadata = client.UploadFile(
        file_name, bucket_name, object_name, gcs::IfGenerationMatch(0));
    if (!metadata) throw std::runtime_error(metadata.status().message());

    std::cout << "Uploaded " << file_name << " to object " << metadata->name()
              << " in bucket " << metadata->bucket() << "\n";
              // << "\nFull metadata: " << *metadata << "\n";
  }
  //! [upload file] [END storage_upload_file]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

bool hasEnding (std::string const &fullString, std::string const &ending) {
  if (fullString.length() >= ending.length()) {
      return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
  } else {
      return false;
  }
}



int main(int argc, char* argv[]) try {

  // These should be passed in as args
  std::string const bucket_name = "edd23232";
  std::string const raw_data_dir = "unprocessed";
  
  // Create the first tier of the subdir structure (doesn't matter if it exists already)
  boost::filesystem::create_directory(raw_data_dir);
  
  // Setup the GCloud stuff. We need two clients: one to read in the filenames, one to perform the downloads
  namespace gcs = google::cloud::storage;
  auto client = gcs::Client::CreateDefaultClient().value();
  // vector<string> fileList = ListObjectsWithPrefix(client, {bucket_name, raw_data_dir});

  // download client
  google::cloud::StatusOr<gcs::Client> io_client = gcs::Client::CreateDefaultClient();

  if (!io_client) {
    std::cerr << "Failed to create Storage Client, status=" << io_client.status() << "\n";
    return 1;
  }

  // After setting up, wait for a http request
  po::variables_map vm = parse_args(argc, argv);

  if (vm.count("help")) return 0;

  auto address = asio::ip::make_address(vm["address"].as<std::string>());
  auto port = vm["port"].as<std::uint16_t>();
  std::cout << "Listening on " << address << ":" << port << std::endl;

  auto handle_session = [&client, &io_client, &bucket_name, &raw_data_dir](tcp::socket socket) {
    auto report_error = [](be::error_code ec, char const* what) {
      std::cerr << what << ": " << ec.message() << "\n";
    };

    be::error_code ec;
    for (;;) {
      be::flat_buffer buffer;

      // Arrival of a request releases the rest of the code
      be::http::request<be::http::string_body> request;
      be::http::read(socket, buffer, request, ec);
      if (ec == be::http::error::end_of_stream) break;
      if (ec) return report_error(ec, "read");

      // First, get the relevant filenames
      vector<string> fullFileList = ListObjectsWithPrefix(client, {bucket_name, raw_data_dir});
      vector<string> fileList;

      // For every filename, split the string to enable creation of the subdir structure. Use boost to make the dirs
      for(int i=0; i < fullFileList.size(); i++) {
        std::string base = raw_data_dir;
        std::vector<std::string> results;
        boost::split(results, fullFileList[i], [](char c){return c == '/';});

        if (results.size() > 2) { // ie if not just 'unprocessed/*.bin'
          for (int i(1); i < results.size()-1; i++) {
            base += "/";
            base +=  results[1];
            boost::filesystem::create_directory(base);
          }
        }

        // Create absolute paths required by the converter, and download the files into these paths locally
        if (hasEnding (fullFileList[i], ".bin")) {
          std::string abs_dl_path = "/r/" + fullFileList[i];
          // io_client->DownloadToFile(bucket_name, fullFileList[i], abs_dl_path);
          DownloadFile(client, {bucket_name, fullFileList[i], abs_dl_path});
          fileList.push_back(abs_dl_path);
        }
      }

      printVector(fileList);

      // Conversion occurs in place so that a .csv accompanies every .bin in the directory tree
      char delim = ',';
      std::string localPrefix = "/r/un";
      int nFiles = fileList.size();
      for(int i=0; i < nFiles; i++){
        if (convertFile(fileList[i], delim) == 0){

          // Swap'.bin' for '.csv'
          std::string fileToUpload = fileList[i].replace(fileList[i].length()-3, 3, "csv");
          // Chop off '/r/un' to give the bucket filepath
          std::string objectName = fileToUpload.substr(localPrefix.length(), fileToUpload.length() - localPrefix.length());
          // Upload to the processed bucket
          // io_client->UploadFile(fileToUpload, bucket_name, objectName);
          UploadFile(client, {fileToUpload, bucket_name, objectName});
        }
        else {
          cout << fileList[i] << " failed to convert" << endl;
          nFiles --;
        }
      }

      cout << "Conversions complete" << endl << endl;

      // Send the response signalling completion
      be::http::response<be::http::string_body> response{be::http::status::ok, request.version()};
      response.set(be::http::field::server, BOOST_BEAST_VERSION_STRING);
      response.set(be::http::field::content_type, "text/plain");
      response.keep_alive(request.keep_alive());

      // Success if none of the requested conversions failed
      std::string msg;
      if (nFiles == fileList.size()) msg = "Success: ";
      else msg = "Error: only ";
      msg += std::to_string(nFiles);
      msg += " of ";
      msg += std::to_string(fileList.size());
      msg += " files converted\n";

      response.body() = std::move(msg);
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