/*
 * Copyright (C) 2012 Hema Koppula
 */


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <set>

#include <pcl/point_types.h>
#include "includes/point_types.h"
#include "includes/CombineUtils.h"
#include <opencv2/opencv.hpp>

#include "./constants.h"
#include "readData.cpp"
#include "frame.cpp"

using namespace std;
typedef pcl::PointXYZRGB PointT;


int Frame::FrameNum = 0;
bool USE_HOG = false;
bool useHead = true;
bool useTorso = true;
bool useLeftArm = true;
bool useRightArm = true;
bool useLeftHand = true;
bool useRightHand = true;
bool useFullBody = true;
bool useImage = true;
bool useDepth = true;
bool useSkeleton = true;



map<string, string> data_act_map;
map<string, vector<string> > data_obj_map;
map<string, vector<string> > data_obj_type_map;
map<string, set<int> > FrameList;
map<string, map< int, set<int> > > SegmentList;
string dataLocation;

void print_feats(vector<double> &feats, std::ofstream &file) {
  for (size_t i = 0; i < feats.size(); i++) {
    file << "," << feats.at(i);
  }
  file << endl;
}


void getHandObjTraj(Frame &frame, std::ofstream &ofile, std::ofstream &hfile) {
  hfile << frame.sequenceId <<  "," << frame.frameNum << ",l,"
        << frame.skeleton.transformed_joints[6].x << ","
        << frame.skeleton.transformed_joints[6].y << ","
        << frame.skeleton.transformed_joints[6].z << endl;  // left hand
  hfile << frame.sequenceId <<  "," << frame.frameNum << ",r,"
        << frame.skeleton.transformed_joints[7].x << ","
        << frame.skeleton.transformed_joints[7].y << ","
        << frame.skeleton.transformed_joints[7].z << endl;  // left hand
  for (int i = 0; i < frame.objects.size(); i++) {
    ofile << frame.sequenceId  << "," << frame.frameNum << ","
    << frame.objects.at(i).objID << "," << frame.objects.at(i).getCentroid().x
    << "," << frame.objects.at(i).getCentroid().y << ","
    << frame.objects.at(i).getCentroid().z << endl;
  }
}

// print error message

void errorMsg(string message) {
  cout << "ERROR! " << message << endl;
  exit(1);
}

void parseChk(bool chk) {
  if (!chk) {
    errorMsg("parsing error.");
  }
}

void readSegmentsFile() {
  const string labelfile = "Segmentation.txt";
  ifstream file((char*) labelfile.c_str(), ifstream::in);
  string line;
  int count = 0;
  while (getline(file, line)) {
    stringstream lineStream(line);
    string element1, element2;
    parseChk(!getline(lineStream, element1, ';').fail());

    if (element1.compare("END") == 0) {
      break;
    }
    while (getline(lineStream, element2, ';')) {
      int pos = element2.find_first_of(':');
      int cluster = atoi(element2.substr(0, pos).c_str());
      cout << "cluster: " << cluster << " :";
      string rest = element2.substr(pos + 1);
      pos = rest.find_first_of(',');
      while (pos != string::npos) {
        int fnum = atoi(rest.substr(0, pos).c_str());
        cout << fnum << ",";
        SegmentList[element1][cluster].insert(fnum);
        rest = rest.substr(pos + 1);
        pos = rest.find_first_of(',');
      }
      int fnum = atoi(rest.substr(0, pos).c_str());
      cout << fnum << ",";
      SegmentList[element1][cluster].insert(fnum);
      cout << endl;
    }
    cout << "\t" << element1 << endl;
    count++;
  }
  file.close();
}

// read label file to get the frame list for each activity
void readLabelFile() {
  const string labelfile = dataLocation + "labeledFrames.txt";
  ifstream file((char*) labelfile.c_str(), ifstream::in);
  string line;
  int count = 0;
  while (getline(file, line)) {
    stringstream lineStream(line);
    string element1, element2;
    parseChk(!getline(lineStream, element1, ',').fail());
    if (element1.compare("END") == 0) {
      break;
    }
    while (!getline(lineStream, element2, ',').fail()) {
      FrameList[element1].insert(atoi(element2.c_str()));
    }
    count++;
  }
  file.close();
}

// read file that maps data and activity


void readDataActMap(string actfile) {
  const string mapfile = dataLocation + actfile;

  printf("Opening map of data to activity: \"%s\"\n",
          (char*) mapfile.c_str());
  ifstream file((char*) mapfile.c_str(), ifstream::in);

  string line;
  int count = 0;
  while (getline(file, line)) {
    stringstream lineStream(line);
    string element1, element2, element3;
    parseChk(!getline(lineStream, element1, ',').fail());

    if (element1.compare("END") == 0) {
      break;
    }
    parseChk(!getline(lineStream, element2, ',').fail());
    if (element1.length() != 10) {
      errorMsg("Data Act Map file format mismatch..");
    }
    data_act_map[element1] = element2;
    parseChk(!getline(lineStream, element3, ',').fail());  // get actor
    while (!getline(lineStream, element3, ',').fail()) {
      cout << element3 << endl;
      int v = element3.find(":", 0);
      cout << element3.substr(0, v) << endl;
      cout << element3.substr(v+1) << endl;
      data_obj_map[element1].push_back(element3.substr(0, v));
      data_obj_type_map[element1].push_back(element3.substr(v+1));
    }

    cout << "\t" << element1 << " : " << data_act_map[element1] << endl;
    count++;
  }
  file.close();

  if (count == 0) {
    errorMsg("File does not exist or is empty!\n");
  }
  printf("\tcount = %d\n\n", count);
}


// read file that maps data and activity

void readDataActMapOld(string actfile) {
  const string mapfile = dataLocation + actfile;

  printf("Opening map of data to activity: \"%s\"\n",
          (char*) mapfile.c_str());
  ifstream file((char*) mapfile.c_str(), ifstream::in);

  string line;
  int count = 0;
  while (getline(file, line)) {
    stringstream lineStream(line);
    string element1, element2, element3;
    parseChk(!getline(lineStream, element1, ',').fail());
    if (element1.compare("END") == 0) {
      break;
    }
    parseChk(!getline(lineStream, element2, ',').fail());
    if (element1.length() != 10) {
      errorMsg("Data Act Map file format mismatch..");
    }
    data_act_map[element1] = element2;
    parseChk(!getline(lineStream, element3, ',').fail());  // get actor
    while (!getline(lineStream, element3, ',').fail()) {
      data_obj_map[element1].push_back(element3);
    }
    cout << "\t" << element1 << " : " << data_act_map[element1] << endl;
    count++;
  }
  file.close();

  if (count == 0) {
    errorMsg("File does not exist or is empty!\n");
  }
  printf("\tcount = %d\n\n", count);
}

void printData(FILE* pRecFile, vector<double> feats) {
  for (int i = 0; i < feats.size(); i++) {
    if (feats.at(i) == 0)
      fprintf(pRecFile, "%.1f,", feats.at(i));
    else
      fprintf(pRecFile, "%.7f,", feats.at(i));
  }
}

void printData(FILE* pRecFile, double * feats, int numFeats) {
  for (int i = 0; i < numFeats; i++) {
    fprintf(pRecFile, "%.7f,", feats[i]);
  }
}

void printData(FILE* pRecFile, int * feats, int numFeats) {
  for (int i = 0; i < numFeats; i++) {
    fprintf(pRecFile, "%d,", feats[i]);
  }
}

int getCluster(int frameNum, string id) {
  for (map< int, set<int> >::iterator i = SegmentList[id].begin();
       i != SegmentList[id].end(); i++) {
    if (i->second.find(frameNum) != i->second.end()) {
      return i->first;
    }
  }
  return 0;
}

/*
 *
 */
int main(int argc, char** argv) {
  dataLocation = (string) argv[1] + "/";
  string actfile = (string) argv[2];
  string mirrored_dataLocation = "";

  readDataActMap(actfile);

  std::ofstream ofile;
  std::ofstream hfile;
  ofile.open("object_trajectories.txt", ios::app);
  hfile.open("hand_trajectories.txt", ios::app);

  // get all names of file from the map
  vector<string> all_files;
  map<string, string>::iterator it = data_act_map.begin();
  while (it != data_act_map.end()) {
    all_files.push_back(it->first);
    cout << it->first << endl;
    it++;
  }
  printf("Number of Files to be processed = %d\n", all_files.size());

  double **data;  // [JOINT_NUM][JOINT_DATA_NUM];
  int **data_CONF;  // [JOINT_NUM][JOINT_DATA_TYPE_NUM]
  double **pos_data;  // [POS_JOINT_NUM][POS_JOINT_DATA_NUM];
  int *pos_data_CONF;  // [POS_JOINT_NUM]
  data = new double*[JOINT_NUM];
  data_CONF = new int*[JOINT_NUM];
  for (int i = 0; i < JOINT_NUM; i++) {
    data[i] = new double[JOINT_DATA_NUM];
    data_CONF[i] = new int[JOINT_DATA_TYPE_NUM];
  }
  pos_data = new double*[POS_JOINT_NUM];
  pos_data_CONF = new int[POS_JOINT_NUM];
  for (int i = 0; i < POS_JOINT_NUM; i++) {
    pos_data[i] = new double[POS_JOINT_DATA_NUM];
  }

  int ***IMAGE;  // [X_RES][Y_RES]
  IMAGE = new int**[X_RES];
  for (int i = 0; i < X_RES; i++) {
    IMAGE[i] = new int*[Y_RES];
    for (int j = 0; j < Y_RES; j++) {
      IMAGE[i][j] = new int[RGBD_data];
    }
  }

  vector<vector<double> > objData;
  vector<vector<int> > objPCInds;
  string lastActId = "0";
  for (size_t i = 0; i < all_files.size(); i++) {
    int count = 1;

    vector <string> fileList(data_obj_map[all_files.at(i)].size());
    for (size_t j = 0; j < data_obj_map[all_files.at(i)].size(); j++) {
      fileList.at(j) = dataLocation + "/" + all_files.at(i) + "_obj"
                       + data_obj_map[all_files.at(i)].at(j) + ".txt";
    }
    vector <string> objPCFileList(data_obj_map[all_files.at(i)].size());
    for (size_t j = 0; j < data_obj_map[all_files.at(i)].size(); j++) {
      objPCFileList.at(j) = dataLocation + "/objects/" + all_files.at(i)
                            + "_obj" + data_obj_map[all_files.at(i)].at(j)
                            + ".txt";
    }

    // for both mirrored and non mirrored data make j<2;
    // for now use only mirrored
    for (int j = 0; j < 1; j++) {
      Frame::FrameNum = 0;
      bool mirrored = (j == 0) ? false : true;
      bool skipOdd = false;
      const string transformfile = dataLocation + all_files[i]
                                   + "_globalTransform.txt";
      readData* DATA = new readData(dataLocation, all_files[i],
                                    data_act_map, i + 1, mirrored,
                                    mirrored_dataLocation, skipOdd, fileList,
                                    objPCFileList);
      int status = DATA->readNextFrame(data, pos_data, data_CONF, pos_data_CONF,
                                       IMAGE, objData, objPCInds);
      int oldSegNum = 1;
      while (status > 0) {
        Frame frame(IMAGE, data, pos_data, objData, all_files[i], status,
                    transformfile, objPCInds);
        getHandObjTraj(frame, ofile, hfile);
        status = DATA->readNextFrame(data, pos_data, data_CONF, pos_data_CONF,
                                     IMAGE, objData, objPCInds);
        count++;
      }
    }
  }
  // fclose(pRecFile);
  ofile.close();
  hfile.close();
  printf("ALL DONE.\n\n");

  return 0;
}
