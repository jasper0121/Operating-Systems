#include <iostream>
#include <sys/types.h>
#include <pthread.h>
#include <vector>
#include <algorithm>
using namespace std;

vector<string> component = { "aircraft", "battery", "propeller" }; //每個零件對應一個編號(index)
// producer[0]不使用，producer[1] ~ producer[3]代表3個生產站
vector<vector<int>> producer(4, vector<int>(4)); // producer[i][0] ~ producer[i][2]分別對應3種零件，producer[i][3] == 空拍機
//配件分配者，dispatcher[i][0] ~ dispatcher[i][2]代表分配對應的3種零件到前台的數量，dispatcher[1] == 進階功能的dispatcher B
vector<vector<int>> dispatcher(2, vector<int>(3));
pthread_mutex_t mutex[2]; //互斥鎖，mutex[0]用在producer、mutex[1]用在進階功能時的dispatcher的A與B
vector<int> platform(3); //前台，配件分配者將零件分配至此，生產站從這裡取得零件
int advanced; //是否進階功能
int drone = 0; //總空拍機數

void* factory(void* input)
{
	size_t i = (size_t)input; //將輸入進來的參數轉成size_t
	if (1 <= i && i <= 3) //若i在1 ~ 3之間
	{
		while (drone < 50) //若完成50個空拍機才會跳出
		{
			pthread_mutex_lock(&mutex[0]); //3個producer中，若其中一個先搶先lock，則另外2個會在這裡被堵塞
			if (drone < 50) //一旦解鎖，另外2個producer之一可能會瞬間進來，由於解鎖之前有可能是生成第50個空拍機，故在此要再次確認是否符合條件
			{
				if (i != 1 && producer[i][0] == 0 && platform[0]) //若producer 2或3當前缺少aircraft而前台有
				{
					cout << "Producer " << i << ": get aircraft\n";
					producer[i][0]++; //則從前台拿過來
					platform[0]--;
				}

				for (int c = 1; c <= 2; c++) //若所有producer當前缺少battery或propeller
				{
					if (producer[i][c] == 0 && platform[c])
					{
						cout << "Producer " << i << (i == 1 ? " (aircraft)" : "") << ": get " << component[c] << "\n";
						producer[i][c]++; //則從前台拿過來
						platform[c]--;
					}
				}

				if (producer[i][0] && producer[i][1] && producer[i][2]) //若當前的producer已經收集好3種零件，則製成空拍機
				{
					cout << "Producer " << i << (i == 1 ? " (aircraft)" : "") << ": OK, " << ++producer[i][3] << " drone(s)\n";
					if (i != 1) //producer 1的aircraft不需歸零
					{
						producer[i][0] = 0;
					}

					producer[i][1] = producer[i][2] = 0;
					drone++; //總空拍機數 + 1
				}
			}

			pthread_mutex_unlock(&mutex[0]); //解鎖
		}
	}
	else //i == 0代表dispatcher A，4代表dispatcher B
	{
		for (int rd; drone < 50;) //若完成50個空拍機才會跳出
		{
			if (platform[rd = rand() % 3] == 0) //每次隨機生成0 ~ 2，若平台當前沒有對應的零件
			{
				if (advanced) //進階功能
				{
					switch (rd)
					{
					case 0:
						pthread_mutex_lock(&mutex[1]); //2個dispatcher都能製造aircraft，建立mutex看誰能搶先lock
						if (platform[0] == 0) //2次檢查，再次確認是否符合條件，防止另一個dispatcher在條件不合下進行存取
						{
							cout << "Dispatcher " << (i ? "B" : "A") << ": aircraft\n";
							platform[0] = 1; //提供aircraft至前台
							dispatcher[i ? 1 : 0][0]++; //紀錄dispatcher A或B總共製造多少aircraft
						}

						pthread_mutex_unlock(&mutex[1]);
						break;
					case 1:
						if (!i) //只有dispatcher A會製造battery
						{
							cout << "Dispatcher A: battery\n";
							platform[1] = 1; //提供battery至前台
							dispatcher[0][1]++; //紀錄dispatcher A總共製造多少battery
						}

						break;
					case 2:
						if (i) //只有dispatcher A會製造propeller
						{
							cout << "Dispatcher B: propeller\n";
							platform[2] = 1; //提供propeller至前台
							dispatcher[1][2]++; //紀錄dispatcher B總共製造多少propeller
						}

						break;
					}
				}
				else //基本功能
				{
					cout << "Dispatcher: " << component[rd] << "\n"; //印出即將要產生哪種零件
					platform[rd] = 1; //提供該零件至前台
					dispatcher[0][rd]++; //紀錄dispatcher總共製造多少該零件
				}
			}
		}
	}

	return NULL;
}

int main(int argc, char* argv[])
{
	if (argc != 3) //從命令列讀入檔名參數，若輸入的參數數量不符則印出錯誤訊息並結束
	{
		cout << "Input error\n";
		pthread_exit(0);
	}

	advanced = atoi(argv[1]); //argv[1] == 0代表基本功能，1代表進階功能
	if (!(0 <= advanced && advanced <= 1)) //若輸入不再範圍內則印出錯誤訊息並結束
	{
		cout << "Please enter 0 or 1 for the first parameter\n";
		pthread_exit(0);
	}

	if (!(0 <= atoi(argv[2]) && atoi(argv[2]) <= 100)) //若輸入不在範圍內則印出錯誤訊息並結束
	{
		cout << "Please enter 0 ~ 100 for the second parameter\n";
		pthread_exit(0);
	}

	srand(atoi(argv[2])); //亂數種
	pthread_mutex_init(&mutex[0], NULL); //producer的mutex初始化
	pthread_mutex_init(&mutex[1], NULL); //dispatcher的mutex初始化
	producer[1][0] = 1; //producer 1常駐擁有aircraft
	vector<pthread_t> multithread(4 + advanced); //建立multithread，若為進階功能則多加1條代表dispatcher B
	for (size_t i = 0; i < multithread.size(); i++) //建立多執行緒，1 ~ 3代表producer、0和4代表dispatcher A和B
	{
		pthread_create(&multithread[i], NULL, factory, (void*)i);
	}

	for (size_t i = 0; i < multithread.size(); i++)
	{
		pthread_join(multithread[i], NULL); //等待執行緒完成工作
	}

	pthread_mutex_destroy(&mutex[0]); //producer的mutex銷毀
	pthread_mutex_destroy(&mutex[1]); //dispatcher的mutex銷毀

	if (advanced) //進階功能
	{
		for (int d = 0; d < 2; d++) //印出dispatcher A和B各自準備了多少各種零件
		{
			for (int i = 0; i < 3; i++)
			{
				cout << "dispatcher " << (d ? "B" : "A") << "(" << component[i] << "): " << dispatcher[d][i] << "\n";
			}
		}
	}
	else //基本功能
	{
		for (int i = 0; i < 3; i++) //印出dispatcher準備了多少各種零件
		{
			cout << "dispatcher (" << component[i] << "): " << dispatcher[0][i] << "\n";
		}
	}

	//存每個producer各自製造多少空拍機，因接下來要用空拍機的數量做排序，故將其值放在first，producer編號放在second
	vector<pair<int, int>> eachDrone = { {producer[1][3], 1}, {producer[2][3], 2}, {producer[3][3], 3} };
	sort(eachDrone.begin(), eachDrone.end(), greater<>()); //以空拍機的數量由大到小排序
	for (int i = 0; i < 3; cout << "producer " << eachDrone[i].second << ": " << eachDrone[i++].first << " drone(s)\n"); //印出各自製造多少空拍機
	pthread_exit(0); //執行緒終止
	return 0;
}