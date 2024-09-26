#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <atomic>
#include <thread>

// ������������������
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	//�ؼ���
	template<typename T>
	Any(T data) : pbase_(std::make_unique<Derive<T>>(data))
	{}
	//�õ��洢��data
	template<typename T>
	T cast_()
	{
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(pbase_.get());
		if (ptr == nullptr)
		{
			throw "type is unmatch!";
		}
		return ptr->data_;
	}
private:
	//��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	//����������
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data){}
		T data_; //��������
	};
	//����ָ��
	std::unique_ptr<Base> pbase_;
};

//ʵ���ź�����
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	//����һ���ź���
	void post()
	{
		std::unique_lock<std::mutex> lck(mtx_);
		resLimit_++;
		cv_.notify_all();
	}
	//����һ���ź���
	void wait()
	{
		std::unique_lock<std::mutex> lck(mtx_);
		cv_.wait(lck, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cv_;
};

class Result;

//�������
class Task
{
public:
	Task();
	~Task() = default;
	void setResult(Result* res);
	void exec();
	virtual Any run() = 0;
private:
	Result* result_;
};


//�õ�����ֵ
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid)
		: task_(task)
		, isValid_(isValid)
	{
		task_->setResult(this);
	}
	~Result() = default;
	//�ѷ���ֵ���ý�any
	void setVal(Any any)
	{
		any_ = std::move(any);
		sem_.post();
	}
	//�ṩ�û����ýӿ�
	Any get()
	{
		if (!isValid_)
		{
			return "";
		}
		sem_.wait();
		return std::move(any_);
	}
private:
	Any any_; //�洢����ֵ
	Semaphore sem_; //ȷ�����ִ���߳������߳�ͨ��
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ��������� 
	std::atomic_bool isValid_;
};

// �̳߳�״̬
enum class PoolMode
{
	MODE_FIXED, //�̶��߳�����
	MODE_CACHED, //�߳������ɶ�̬����
};



//�߳���
class Thread
{
public:
	using func = std::function<void(int)>;
	Thread(func fun);
	~Thread();
	//�����߳�
	void start();
	//��ȡ��ǰ�߳�id
	int getId();
private:
	func func_;
	static int generateId_;
	int threadId_; //��ǰ�߳�id
};

//�̳߳���
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();
	// ���ù���ģʽ
	void setMode(PoolMode mode);
	// ����������������������
	void setTaskQue(int maxsize);
	
	// �ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = 4);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �̴߳�������
	void threadFunc(int threadid);
	// ���pool������״̬
	bool checkRunningState() const;
private:
	//std::vector<std::unique_ptr<Thread>> threadVec_; // �߳�����
	std::unordered_map<int, std::unique_ptr<Thread>> threadMap_;
	int initThreadSize_; // ��ʼ�߳�����
	int maxThreadSize_; // ����߳�����
	std::atomic_int idleThreadSize_; // ��¼�����̵߳�����
	std::atomic_int curThreadSize_;	// ��¼��ǰ�̳߳������̵߳�������

	std::queue<std::shared_ptr<Task>> taskQue_; // �������
	int maxTaskSize_; // �����������
	std::atomic_int taskSize_; // ��������  Ҫ��֤���ж���������Ƿ���ʱ��ԭ����

	std::mutex taskQueMtx_; // ��֤������̰߳�ȫ����
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	std::condition_variable exitCond_; // �ȵ��߳���Դȫ������

	PoolMode poolMode_; // ��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_; // ��ʾ��ǰ�̳߳ص�����״̬
};

#endif