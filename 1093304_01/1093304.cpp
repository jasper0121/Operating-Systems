#include <iostream>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>
using namespace std;

struct share_memory
{
	int x, y, last_pid, turn; //砲彈要攻打的xy座標、紀錄最後攻打的是父還是子程序、回合(-1 == 未開始、0 == 子回合、1 == 父回合、2 == 結束)
	bool hit; //記錄炸射狀態
};

int main(int argc, char* argv[])
{
	const char* memname = "plog";
	int fd = shm_open(memname, O_CREAT | O_TRUNC | O_RDWR, 0666); //建立share memory
	if (fd < 0) //若fd < 0則代表開啟有問題
	{
		cout << "shm open error\n";
		shm_unlink(memname); //刪除記憶體共享檔案
		return 0;
	}

	ftruncate(fd, sizeof(share_memory)); //將引數fd指定的檔案大小改為引數share_memory指定的大小
	//把文件內容映射到一段記憶體上，對這段記憶體的讀取等同對文件的讀取
	share_memory* shm = (share_memory*)mmap(NULL, sizeof(share_memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	shm->turn = -1; //初始回合設為0代表未開始
	int p1, p2, p3; //前2數為父子的亂數種
	cout << "> prog1 ";
	cin >> p1 >> p2 >> p3;

	pid_t pid = fork(); //建立子程序
	if (pid > 0) //若為父程序
	{
		srand(p1); //放入亂數種並印出
		cout << "[" << getpid() << " Parent]: Random Seed: " << p1 << endl;
	}
	else if (pid == 0) //若為子程序
	{
		srand(p2); //放入亂數種並印出
		cout << "[" << getpid() << " Child]: Random Seed: " << p2 << endl;
	}

	const int dir[5]{ 0, 1, 0, -1, 0 }; //方向，找上下左右用
	int Map[4][4]{}, x = rand() % 4, y = rand() % 4, ex, ey; //4 x 4地圖、亂數找船頭的座標、船尾座標
	Map[x][y] = 1; //船頭所在位置設為1
	for (int i = 0; i < 4; i++) //找船尾位置
	{
		if (x + dir[i] < 0 || x + dir[i] == 4 || y + dir[i + 1] < 0 || y + dir[i + 1] == 4) //若要制定船尾的座標超出範圍
		{
			continue; //則直接找下個方向
		}

		Map[ex = x + dir[i]][ey = y + dir[i + 1]] = 1; //找到船尾的位置並將其設為1
		break;
	}

	//父子程序分別印出各自的船座標，因parent似乎執行較快，故讓child開啟parent的回合
	cout << "[" << getpid() << " " << (pid ? "Parent" : (shm->turn = 1, "Child")) << "]: The gunboat: (" << x << "," << y << ")(" << ex << "," << ey << ")\n";
	int bombNum = 0; //雙方各自使用的炸彈數
	for (int shipHit = 0; shm->turn != 2;) //各自的船被擊中的次數，若為2則代表整艘被擊沉
	{
		if (pid > 0) //父程序會跑的if
		{
			if (shm->turn == 1) //若turn == 1，代表輪到父程序的回合
			{
				if (bombNum) //一開始只會發射砲彈，除此之外每次到父的回合時，都會先判斷是否被擊中，再發射砲彈
				{
					if (Map[shm->x][shm->y] == 1) //若此時share memory中的座標就是父的船還未擊沉的部分
					{
						shm->hit = true; //被擊中，狀態設為true
						Map[shm->x][shm->y] = 0; //代表擊中，父的該位置設為0
						cout << "[" << getpid() << " Parent]: hit"; //輸出擊中的訊息
						if (++shipHit == 2) //因有擊中，故shipHit先 + 1，再判斷若船艦的2個部分皆已被擊沉
						{
							cout << " and sinking\n"; //則再輸出已擊沉
							shm->turn = 2; //此時轟炸結束
							break; //並跳出迴圈
						}

						cout << "\n";
					}
					else //否則代表未擊中
					{
						shm->hit = false; //未擊中，狀態設為false
						cout << "[" << getpid() << " Parent]: missed\n";
					}
				}

				shm->last_pid = getpid(); //每次轟炸前，share memory 先記錄當前是誰轟炸
				shm->x = rand() % 4; //隨機選擇轟炸座標
				shm->y = rand() % 4;
				bombNum++; //丟出一顆砲彈，砲彈數 + 1
				cout << "[" << getpid() << " Parent]: bombing (" << shm->x << "," << shm->y << ")\n"; //輸出發射砲彈的訊息
				shm->turn = 0; //砲彈發射後輪到子程序的回合
			}
		}
		else if (pid == 0) //子程序會跑的if
		{
			if (shm->turn == 0) //若turn == 0，代表輪到子程序的回合
			{

				if (Map[shm->x][shm->y] == 1) //若此時share memory中的座標就是子的船還未擊沉的部分
				{
					shm->hit = true; //被擊中，狀態設為true
					Map[shm->x][shm->y] = 0; //代表擊中，子的該位置設為0
					cout << "[" << getpid() << " Child]: hit"; //輸出擊中的訊息
					if (++shipHit == 2) //因有擊中，故shipHit先 + 1，再判斷若船艦的2個部分皆已被擊沉
					{
						cout << " and sinking\n"; //則再輸出已擊沉
						shm->turn = 2; //此時轟炸結束
						exit(0); //結束子程序
					}

					cout << "\n";
				}
				else //否則代表未擊中
				{
					shm->hit = false; //未擊中，狀態設為false
					cout << "[" << getpid() << " Child]: missed\n";
				}

				shm->last_pid = getpid(); //每次轟炸前，share memory 先記錄當前是誰轟炸
				shm->x = rand() % 4; //隨機選擇轟炸座標
				shm->y = rand() % 4;
				bombNum++; //丟出一顆砲彈，砲彈數 + 1
				cout << "[" << getpid() << " Child]: bombing (" << shm->x << "," << shm->y << ")\n"; //輸出發射砲彈的訊息
				shm->turn = 1; //砲彈發射後輪到父程序的回合
			}
		}
	}

	if (pid > 0) //最後由父程序印出是誰獲勝和其所消耗的砲彈數
	{
		cout << "[" << getpid() << " Parent]: " << shm->last_pid << " wins with " << bombNum << " bombs\n";
	}

	munmap(shm, sizeof(share_memory)); //解除記憶體映射
	shm_unlink(memname); //刪除記憶體共享檔案
	return 0;
}