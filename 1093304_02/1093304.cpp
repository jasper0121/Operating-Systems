#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/types.h>
#include <pthread.h>
#include <cmath>
#include <vector>
#include <string>
#include <map>
using namespace std;

vector<string> id, document;   //文件的ID、存放各個文件句子
map<string, vector<int>> word; //存每個句子中的單字，key存單字、val存key在每個句子中所出現的次數
vector<double> avg;			   //每組詞頻向量的平均

string only_word(string &sentence) //刪除句子中的標點符號
{
	if (sentence.back() == '\r') //若該句結尾有\r則將其刪除
	{
		sentence.pop_back();
	}

	for (size_t i = 0; i < sentence.size(); i++) //若句子中有標點符號則將其改為空格
	{
		if (!isalnum(sentence[i]))
		{
			sentence[i] = ' ';
		}
	}

	return sentence;
}

bool only_letter(const string &word) //回傳當前的單字是否只用英文字母組成
{
	for (size_t i = 0; i < word.size(); i++)
	{
		if (!isalpha(word[i])) //若當前單字中的字元不是英文字母
		{
			return false; //則回傳false
		}
	}

	return true; //單字確定只由英文字母組成，回傳true
}

void *avg_cosine(void *input)
{
	size_t cur = (size_t)input;								//將輸入進來的參數轉成size_t，current == 當前的詞頻向量
	pair<timespec, timespec> CPUtime;						//計算CPU時間，pair中的first和second分別代表start和end
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &CPUtime.first); //紀錄start的CPU時間
	unsigned long int tid = pthread_self();					//紀錄當前的tid
	cout << "[TID=" << tid << "] DocID: " << id[cur] << " [";
	for (auto it = word.begin(); it != word.end(); it++) //印出詞頻向量
	{
		cout << (it == word.begin() ? "" : ",") << it->second[cur];
	}

	cout << "]\n";
	for (size_t tar = 0; tar < document.size(); tar++) // target == 對象的詞頻向量
	{
		if (cur != tar) //若2詞頻向量不相同(不能自己和自己做運算)
		{
			double curSum = 0, tarSum = 0, cur_x_tar_Sum = 0, result; // 2向量的平方總和、2向量的相乘總和、2向量的最終Cos結果
			for (auto it = word.begin(); it != word.end(); it++)	  //計算所有的Cos值
			{
				curSum += it->second[cur] * it->second[cur];
				tarSum += it->second[tar] * it->second[tar];
				cur_x_tar_Sum += it->second[cur] * it->second[tar];
			}

			avg[cur] += (result = cur_x_tar_Sum / (sqrt(curSum) * sqrt(tarSum)));													  //所有Cos的值加總
			cout << fixed << setprecision(4) << "[TID=" << tid << "] cosine(" << id[cur] << "," << id[tar] << ")=" << result << "\n"; //印出當前的Cos值
		}
	}

	cout << "[TID=" << tid << "] Avg_cosine: " << (avg[cur] /= document.size() - 1) << "\n"; //計算並印出平均餘弦相似係數
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &CPUtime.second);								 //紀錄end的CPU時間
	//印出的時間差即CPU時間，由於tv_nsec == 奈米秒，故除以CLOCKS_PER_SEC(在linux環境中等於1000000)則等於毫秒(ms)
	cout << "[TID=" << tid << "] CPU time: " << double(CPUtime.second.tv_nsec - CPUtime.first.tv_nsec) / CLOCKS_PER_SEC << "ms\n";
	return NULL;
}

int main(int argc, char *argv[])
{
	pair<timespec, timespec> main_CPUtime;						 // main thread的CPU時間
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &main_CPUtime.first); //紀錄start的CPU時間
	if (argc != 2) //從命令列讀入檔名參數，若輸入的參數數量不符則印出錯誤訊息並結束
	{
		cout << "Input error\n";
		pthread_exit(0);
	}

	ifstream inFile(argv[1]); //讀檔
	if (!inFile)			  //若沒讀到檔
	{
		cout << "File could not be opened\n";
		pthread_exit(0);
	}

	for (string input; !inFile.eof();) //若還未到檔案結尾
	{
		getline(inFile, input);			//一行一行地從inFile讀入input
		id.push_back(only_word(input)); //將讀取到的ID記錄到id中
		getline(inFile, input);
		document.push_back(only_word(input)); //將讀取到的句子記錄到document中
	}

	inFile.close(); //讀檔關閉
	for (size_t i = 0; i < document.size(); i++)
	{
		stringstream getWord(document[i]);				  //擷取句子中的單字
		for (string input; getline(getWord, input, ' ');) //若能擷取到單字
		{
			if (only_letter(input) && input != "") //若當前單字僅由字母組成，且該單字 != 空字串
			{
				auto it = word.insert({input, vector<int>(document.size())}); //則將該單字加入map中，vector空間 == 句子數，存該單字在對應的句子中出現的次數
				it.second ? it.first->second[i] = 1 : word[input][i]++; //若it.second == true，代表此單字是新增進來的，則在對應的句子index中放入1；否則代表該單字之前已存在過，則在對應的句子index中 + 1
			}
		}
	}

	vector<pthread_t> multithread(document.size()); //有幾份文件句子就創建幾個thread
	avg.assign(document.size(), double());			//每個句子都會和其他句子算出Cos值並最後做平均，故存放平均數的空間 == 文件數
	for (size_t i = 0; i < multithread.size(); i++) //建立多執行緒
	{
		pthread_create(&multithread[i], NULL, avg_cosine, (void *)i);
		cout << "[Main thread]: create TID: " << multithread[i] << ", DocID: " << id[i] << "\n";
	}

	for (size_t i = 0; i < multithread.size(); i++)
	{
		pthread_join(multithread[i], NULL); //等待執行緒完成工作
	}

	double avg_max = 0, index;				//所有平均中的最大值就是關鍵文件、index == 關鍵文件所在的位置
	for (size_t i = 0; i < avg.size(); i++) //找出最大值並記錄該值所在的位置
	{
		avg_max = max(avg_max, avg[index = i]);
	}

	cout << "[Main thread] KeyDocID: " << id[index] << " Highest Average Cosine: " << avg_max << "\n"; //印出關鍵文件的ID及其值
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &main_CPUtime.second);									   //紀錄end的CPU時間	
	cout << "[Main thread] CPU time: " << double(main_CPUtime.second.tv_nsec - main_CPUtime.first.tv_nsec) / CLOCKS_PER_SEC << "ms\n"; //印出main thread的CPU時間
	pthread_exit(0); //執行緒終止
	return 0;
}