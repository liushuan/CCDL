
//����һ��lstm+cnn��ocr����
//2017��7��14�� 12:23:54
//wish

#include "caffe.pb.h"
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include "classification-c.h"
#include <thread>
#include <io.h>
#include <fcntl.h>
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
using namespace std;

#ifdef _DEBUG
#pragma comment(lib, "libprotobufd.lib")
#else
#pragma comment(lib, "libprotobuf.lib")
#endif
#pragma comment(lib, "classification_dll.lib")

vector<char> readFile(const char* file){
	vector<char> data;
	FILE* f = fopen(file, "rb");
	if (!f) return data;

	int len = 0;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len > 0){
		data.resize(len);
		fread(&data[0], 1, len, f);
	}
	fclose(f);
	return data;
}

void loadCodeMap(const char* file, vector<string> & out){
	ifstream infile(file);
	string line;
	while (std::getline(infile, line)){
		out.push_back(line);
	}
}

string getLabel(const vector<string>& labelMap, int index){
	if (index < 0 || index >= labelMap.size())
		return "*";

	return labelMap[index];
}

int argmax(float* arr, int begin, int end)
{
	try
	{
		int mxInd = 0;
		float acc = -9999;
		for (int i = begin; i < end; i++)
		{
			if (acc < arr[i])
			{
				mxInd = i;
				acc = arr[i];
			}
		}
		return mxInd - begin;
	}
	catch (exception)
	{

		return -1;
	}

}
CRITICAL_SECTION  g_csThreadCode;
HANDLE            g_hThreadParameter;
long lReleaseCount = 0;
const int MAX_THREAD = 90;
int ��ȷ = 0, ���� = 0;
int num_output = 0;
int time_step = 0;
vector<string> labelMap;
TaskPool* classifierHandle;
string strPath;
void doproc(const string strFile)
{
	string strFileName = strFile;
	string strFileNameAll = strPath + strFileName;
	//printf("��ʼһ���߳�%s", strFileName.c_str());
	BlobData* ������� = NULL;
	try
	{
		vector<char> data = readFile(strFileNameAll.c_str());
		for (int i = 0; i < 30; i++)
		{
			������� = forwardByTaskPool(classifierHandle, &data[0], data.size(), "premuted_fc");
			if (������� != NULL)break;
			Sleep(30);
		}
		if (������� != NULL)
		{
			int �հױ�ǩ���� = num_output - 1;
			int prev = �հױ�ǩ����;
			int o = 0;
			string rt = "";
			int len = getBlobLength(�������);
			if (len != 0)
			{
				float* permute_fc = new float[len];
				try
				{
					cpyBlobData(permute_fc, �������);

					for (int i = 1; i < time_step; i++)
					{
						o = argmax(permute_fc, (i - 1) * num_output, i * num_output);

						if (o != �հױ�ǩ���� && prev != o && o > -1 && o < num_output)
						{
							rt += labelMap[o];
						}

						prev = o;
					}
				}
				catch (...)
				{
				}
				delete[] permute_fc;
				string strS = strFileName.substr(0, strFileName.find('_', 0));
				string s;
				EnterCriticalSection(&g_csThreadCode);
				if (strS == rt)
				{
					��ȷ++;
					s = "��ȷ";
				}
				else
				{
					����++;
					s = "����";
					printf("ʶ��Ľ���ǣ�%s\t%s\t��ȷ��=%s\n", rt.c_str(), s.c_str(), strFileName.c_str());
				}
				LeaveCriticalSection(&g_csThreadCode);
			}
		}
		else
		{
			printf("��������ǿ�:%s,��С=%d\n", strFile.c_str(), data.size());
		}
	}
	catch (...)
	{
		printf("�쳣");
	}
	if(�������)releaseBlobData(�������);
	ReleaseSemaphore(g_hThreadParameter, 1, &lReleaseCount);
}

void FindFile()
{
	WIN32_FIND_DATAA  findData = { 0 };
	string strFindPath = strPath + "*.bmp";
	//���ҵ�һ���ļ�  
	HANDLE hFindFine = FindFirstFileA(strFindPath.c_str(), &findData);
	if (INVALID_HANDLE_VALUE == hFindFine)
		return;
	InitializeCriticalSection(&g_csThreadCode);
	g_hThreadParameter = CreateSemaphore(NULL, MAX_THREAD, MAX_THREAD, NULL);
	
	//ѭ�������ļ�
	int i = 0;
	do
	{
		WaitForSingleObject(g_hThreadParameter, INFINITE);
		string str = findData.cFileName;
		thread(doproc, str).detach();
		//Sleep(1);
		//doproc(findData.cFileName);
	} while (FindNextFileA(hFindFine, &findData));
	while (lReleaseCount < MAX_THREAD - 1)
	{
		Sleep(100);
	}
	Sleep(1000);
	DeleteCriticalSection(&g_csThreadCode);
	CloseHandle(g_hThreadParameter);
	printf("��ȷ=%d��������=%d������ȷ��=%f\n", ��ȷ, ����, (double)��ȷ / (���� + ��ȷ));
	//�ر��ļ��������  
	FindClose(hFindFine);
}

//��ȡdeploy.prototxt�����е�num_output��time_step����ֵ
bool __stdcall GetProtoParam2(const char* filename){
	caffe::NetParameter proto;
	int fd = open(filename, O_RDONLY);
	if (fd == -1)return false;
	google::protobuf::io::FileInputStream* input = new google::protobuf::io::FileInputStream(fd);
	bool success = google::protobuf::TextFormat::Parse(input, &proto);
	delete input;
	close(fd);
	num_output = 0;
	time_step = 0;
	if (success)
	{
		for (int i = 0; i < proto.layer_size(); i++)
		{
			caffe::LayerParameter layerp = proto.layer(i);
			if (layerp.name() == "fc1x")
			{
				num_output = layerp.inner_product_param().num_output();
			}
			if (layerp.name() == "indicator")
			{
				time_step = layerp.continuation_indicator_param().time_step();
			}
			if (num_output != 0 && time_step != 0)break;
		}
	}
	return num_output != 0 && time_step != 0;
}
void FindFile2(const std::string& strPath, Classifier* classifierHandle)
{

	WIN32_FIND_DATAA  findData = { 0 };
	string strFindPath = strPath + "*.bmp";
	//���ҵ�һ���ļ�  
	HANDLE hFindFine = FindFirstFileA(strFindPath.c_str(), &findData);
	if (INVALID_HANDLE_VALUE == hFindFine)
		return;
	//ѭ���ݹ�����ļ�  
	int ��ȷ = 0, ���� = 0;
	do
	{
		vector<char> data = readFile((strPath + findData.cFileName).c_str());
		BlobData* �������;
		for (int i = 0; i < 30; i++)
		{
			forward(classifierHandle, &data[0], data.size());// , "premuted_fc");
			������� = getBlobData(classifierHandle, "premuted_fc");
			if (������� != NULL)break;
			Sleep(30);
		}
		if (������� != NULL)
		{
			//int time_step = 19;// 15
			int �հױ�ǩ���� = num_output - 1; //25;//34; //�����ʾ�����ַ����������»��ߵĿհ׷�
			//int �ַ����� = �հױ�ǩ���� + 1;
			int prev = �հױ�ǩ����;
			int o = 0;
			string rt = "";
			int len = getBlobLength(�������);
			if (len != 0)
			{
				float* permute_fc = new float[len];
				try
				{
					cpyBlobData(permute_fc, �������);

					for (int i = 1; i < time_step; i++)
					{
						o = argmax(permute_fc, (i - 1) * num_output, i * num_output);

						if (o != �հױ�ǩ���� && prev != o && o > -1 && o < num_output)
						{
							rt += labelMap[o];
						}

						prev = o;
					}
				}
				catch (...)
				{
				}
				delete[] permute_fc;
				string strS = findData.cFileName;
				strS = strS.substr(0, strS.find('_', 0));
				string s;
				if (strS == rt)
				{
					��ȷ++;
					s = "��ȷ";
				}
				else
				{
					����++;
					s = "����";
					printf("ʶ��Ľ���ǣ�%s\t%s\t��ȷ��=%s\n", rt.c_str(), s.c_str(), strS.c_str());
				}
			}
			releaseBlobData(�������);
		}
	} while (FindNextFileA(hFindFine, &findData));
	printf("��ȷ=%d��������=%d������ȷ��=%f", ��ȷ, ����, (double)��ȷ / (���� + ��ȷ));
	//�ر��ļ��������  
	FindClose(hFindFine);
}

void ss(int avgc, char **avgv)
{
	try
	{
		if (!GetProtoParam2(avgv[1]))
		{
			printf("��ȡproto����ʧ��\n");
			return;
		}
		printf("num_output = %d, time_step = %d\n", num_output, time_step);
		disableErrorOutput();
		Classifier* classifierHandle = createClassifier(avgv[1], avgv[2]);// , 1, 0, 0, 0, 0, 16);
		loadCodeMap(avgv[3], labelMap);
		/*vector<char> data = readFile(avgv[4]);
		if (data.empty()){
		printf("�ļ�������ô��\n");
		releaseTaskPool(classifierHandle);
		return;
		}*/
		FindFile2(avgv[4], classifierHandle);
		releaseClassifier(classifierHandle);
	}
	catch (...)
	{
		printf("�쳣\n");
	}
}
void main(int avgc, char** avgv){
	//return ss(avgc, avgv);
	//��ֹcaffe�����Ϣ
	if (avgc < 5)return;
	try
	{
		if (!GetProtoParam2(avgv[1]))
		{
			printf("��ȡproto����ʧ��\n");
			return;
		}
		printf("num_output = %d, time_step = %d\n", num_output, time_step);
		disableErrorOutput();
		classifierHandle = createTaskPool(avgv[1], avgv[2], 1, 0, 0, 0, 0, MAX_THREAD);
		loadCodeMap(avgv[3], labelMap);
		/*vector<char> data = readFile(avgv[4]);
		if (data.empty()){
			printf("�ļ�������ô��\n");
			releaseTaskPool(classifierHandle);
			return;
		}*/
		strPath = avgv[4];
		FindFile();
		releaseTaskPool(classifierHandle);
	}
	catch (...)
	{
		printf("�쳣\n");
	}
}