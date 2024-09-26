#include <iostream>
#include "ThreadPool.h"
using namespace std;

using uLong = unsigned long long;
class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin)
		, end_(end)
	{
	}

	Any run()
	{
		//std::cout << "begin work tid:" << std::this_thread::get_id() 
		//	<< std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		uLong sum = 0;
		for (int i = begin_; i <= end_; ++i)
		{
			sum += i;
		}
		//std::cout << "end work tid:" << std::this_thread::get_id()
		//	<< std::endl;	
		return sum;
	}
private:
	int begin_;
	int end_;
};


int main()
{
	{
		ThreadPool pool;
		pool.start(2);
		pool.setMode(PoolMode::MODE_FIXED);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		uLong sum1 = res1.get().cast_<uLong>();  // get������һ��Any���ͣ���ôת�ɾ���������أ�
		uLong sum2 = res2.get().cast_<uLong>();
		uLong sum3 = res3.get().cast_<uLong>();
		// Master - Slave�߳�ģ��
		// Master�߳������ֽ�����Ȼ�������Slave�̷߳�������
		// �ȴ�����Slave�߳�ִ�������񣬷��ؽ��
		// Master�̺߳ϲ����������������
		cout << (sum1 + sum2 + sum3) << endl;
	}
	getchar();
}
