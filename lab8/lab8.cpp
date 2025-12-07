#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <fstream>

#define PT_NUM 9

using namespace std;

pthread_mutex_t mutex;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int signal = 0;

void* wr(void *arg)
{
     int str = 1;
     char buffer[80];

     while(1)
     {
        pthread_mutex_lock(&mutex);
	
        time_t seconds = time(NULL);
        tm* timeinfo = localtime(&seconds);
        const char* format = "%A, %B %d, %Y %I:%M:%S";
        setlocale(LC_ALL, "");
        strftime(buffer, 80, format, timeinfo);
        cout << "Поток записи: " << pthread_self() << endl;
        ofstream file("Время.txt", ios::app);
        file << "Строка: " << str << ", ";
        file << "дата: " << buffer << endl;
        file.close();
        str++;
	signal = 1;
	pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
	sleep(1);
     }
     pthread_exit(0);
}

void* rd(void *arg)
{
     char buffer[80];
     int str_read = 0;
     int str_write = 0;
     
     while(1)
     {
        pthread_mutex_lock(&mutex);
        while (signal != 1)
	    pthread_cond_wait(&cond, &mutex);
        ifstream file("Время.txt");
        cout << "Поток чтения: " << pthread_self() << endl;;
        while (!file.eof())
        {
            file.getline(buffer, 80);
            cout << buffer << endl;
        }
        file.close();
        pthread_mutex_unlock(&mutex);
	sleep(1);
     }
     pthread_exit(0);
}

int main (int argc, char* argv[])
{
	pthread_t read [PT_NUM];
        pthread_t write;

        pthread_create(&write, NULL, wr, NULL);
	for (int i = 0; i<PT_NUM; ++i)
	{
		pthread_create(&read[i], NULL, rd, NULL);
	}
                
        pthread_join(write, NULL);
        for (int i = 0; i<PT_NUM; ++i)
	{
                pthread_join(read[i], NULL);
	}


        
	return 0;
}
