/*
// This executable can be compiled using g++ csv_conv2.cpp -o CSVConverter.exe
// It recursively works through any subdirectories, collects up any binary file names, then converts them in dir to csv files 
// Alternatively can provide relative filenames as args for conversion: ./CSVConverter.exe -./folder/filename.bin
// Main returns 1 if conversion of any file fails, or if there are no files, so that airflow knows about it
*/ 

#include <iostream>
#include <fstream>
#include <dirent.h>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>

// Get the common structs used for storing motion data
// All the data that we read and store every timestep. 30 bytes
#pragma pack(push, 1)
struct data_t {  
  int16_t imuData[12]; //a0[3], g0[3], a1[3], g1[3]
  uint8_t imuStatus[4]; // a0, g0, a1, g1; 
  uint8_t FSR;      // 8bit 
  uint8_t time; //Reduced from 32bit by storing INTERVAL - all good as long as this doesnt exceed 255ms (4Hz)
  uint8_t prediction;
};
#pragma pack(pop)

const uint16_t dataDim = (512 - 2) / sizeof(data_t);
const uint16_t fillDim = 512 - 2 - dataDim * sizeof(data_t);

using namespace std;


//Data is saved in blocks, so we need that struct for decoding it
struct block_t {
  uint8_t count;
  uint8_t overrun;
  data_t data[dataDim];
  uint8_t fill[fillDim];
};

// FUNCTIONS FOR FILE HANDLING //

// Returns size of file
int getSize (ifstream& myfile)
{
  streampos begin,end;
  begin = myfile.tellg();
  myfile.seekg(0, ios::end);
  end = myfile.tellg();
  myfile.seekg(0, ios::beg);

  return (int) (end-begin);
}

// Prints the detected files to console
void printVector(vector<string> fileList) {
  cout << endl << endl << fileList.size() << " file(s) for conversion:"  << endl;

  for(int i=0; i<fileList.size(); i++){
    cout << fileList[i] << endl;
  }
  cout << endl;
}

int convertFile(string binFileName, char delim){

  ifstream binFile;
  ofstream csvFile, csvTimeFile;
  streampos begin,current;
  data_t datapoint;
  block_t block;
  int fileSize, progress, i, packetSize;
  int state = 1;
  long int counter = 0;
  string csvFileName;

  // Below, we will need the absolute path to the current working dir
//   string path = getexepath();

  // Open bin file
  // Append the dir path to the relative file name picked up earlier, but first chop off the initial '.'
//   binFileName = path + binFileName.erase(0,1);

  binFile.open(binFileName.c_str(), ios::binary);  // change required on mac compiler

  if (binFile.fail()){
    return 1;
  };

  // For progress tracking
  packetSize = sizeof(block);
  fileSize = getSize(binFile);
  begin = binFile.tellg();

  // Create name for csv file
  csvFileName = binFileName;

  csvFileName.replace( csvFileName.find(".")+1, csvFileName.find(".")+4, "csv");
  csvFile.open(csvFileName.c_str()); // change required on mac compiler
  if (csvFile.fail()){
    return 1;
  };

  csvFile << "acc_x_left" << delim << "acc_y_left" << delim << "acc_z_left" << delim << "gyr_x_left" << delim << "gyr_y_left" << delim << "gyr_z_left" << delim;
  csvFile << "acc_x_right" << delim << "acc_y_right" << delim << "acc_z_right" << delim << "gyr_x_right" << delim << "gyr_y_right" << delim << "gyr_z_right" << delim;
  csvFile << "prediction" <<delim;
  csvFile << "FSR" << delim <<  "time_delta" << delim;
  csvFile << "left_acc_mag_status" << delim <<  "left_gyro_status" << delim << "right_acc_mag_status" << delim <<  "right_gyro_status" << endl;
  progress = fileSize/(10*packetSize);

  while( binFile.read((char*) &block, 512) )
  {
    counter++;
    if (counter > progress){
      progress = progress + fileSize/(10*packetSize);
      state++;
    }

//     // Break is reached end of block
    if (block.count == 0) {
      break;
    }

    
    // Write to file
    for (int i = 0; i < block.count; i++) {
      datapoint = block.data[i];

      csvFile << datapoint.imuData[0] << delim << datapoint.imuData[1] << delim << datapoint.imuData[2] << delim;
      csvFile << datapoint.imuData[3] << delim << datapoint.imuData[4] << delim << datapoint.imuData[5] << delim;
      csvFile << datapoint.imuData[6] << delim << datapoint.imuData[7] << delim << datapoint.imuData[8] << delim;
      csvFile << datapoint.imuData[9] << delim << datapoint.imuData[10] << delim << datapoint.imuData[11] << delim;

      csvFile << (int) datapoint.prediction << delim;

      //Need to convert time (uint8_t) to int for iostream: do this with +
      csvFile << (int) datapoint.FSR << delim << +datapoint.time << delim; 

      csvFile << (int) datapoint.imuStatus[0] << delim << (int) datapoint.imuStatus[1] << delim;
      csvFile << (int) datapoint.imuStatus[2] << delim << (int) datapoint.imuStatus[3] << delim <<endl;
    }
  }

    binFile.close();
    csvTimeFile.close();

    return 0;
}
