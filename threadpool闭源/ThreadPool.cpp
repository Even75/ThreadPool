#include "ThreadPool.h"

const int MAX_THREAD_SIZE = 10;
const int MAX_TASK_SIZE = 10;
const int THREAD_MAX_IDLE_TIME = 5;
/*---------ThreadPool::-----------*/
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, maxThreadSize_(MAX_THREAD_SIZE)
	, maxTaskSize_(MAX_TASK_SIZE)
	, taskSize_(0)
	, poolMode_(PoolMode::MODE_FIXED)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, isPoolRunning_(true)
{

}
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	notEmpty_.notify_all();

	std::unique_lock<std::mutex> lck(taskQueMtx_);
	exitCond_.wait(lck, [&]()->bool {return threadMap_.size() == 0; });
}
// ���ù���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}
// ����������������������
void ThreadPool::setTaskQue(int maxsize)
{
	maxTaskSize_ = maxsize;
}
// �ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lck(taskQueMtx_);

	//�ж������������  ��=���ȴ�  ����=���ύ����  
	//��������һ����� �������ÿ1s��ӡһ��  
	if (!notFull_.wait_for(lck, std::chrono::seconds(1)
		, [&]()->bool {return taskSize_ < maxTaskSize_; }))
	{
		std::cout << "taskQue is full, wait ...." << std::endl;
		return Result(sp, false);
	}

	taskQue_.emplace(sp);
	taskSize_++;
	notEmpty_.notify_all();
	
	//�����CATCHģʽ�����жϵ�ǰ���������Ƿ���ڿ����߳�������ǰ�߳������Ƿ�С������߳���
	//��֤���価������߳�ȥ��������
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < maxThreadSize_)
	{
		std::cout << "create new thread ..." << std::endl;
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int id = ptr->getId();
		threadMap_.emplace(id, std::move(ptr));
		threadMap_[id]->start();

		idleThreadSize_++;
		curThreadSize_++;
	}

	return Result(sp, true);
}
// �����̳߳�
void ThreadPool::start(int initThreadSize)
{
	//��ʼ���߳�����
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;//��ǰ�߳�����
	// �����̶߳���
	for (int i = 0; i < initThreadSize_; ++i)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc
												, this, std::placeholders::_1));
		int id = ptr->getId();
		threadMap_.emplace(id, std::move(ptr));
	}

	// ��ʼ���߳��б�
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threadMap_[i]->start();
		idleThreadSize_++;//�����߳�����
	}

}
// �̴߳�����
/*
catchģʽ�£����������ݵ��߳�һֱִ�У��˷Ѳ���Ҫ����Դ�������һ������û�������񣬾����٣�����
*/
void ThreadPool::threadFunc(int threadid)
{
	std::cout << "threadID:" << std::this_thread::get_id() << std::endl;
	auto lastTime = std::chrono::high_resolution_clock().now();
	//��֤�����̶߳��ڹ���������
	while (isPoolRunning_)
	{
		std::shared_ptr<Task> task = nullptr;
		{
			//��ȡ��
			std::unique_lock<std::mutex> lck(taskQueMtx_);

			//���������пգ�������catchģʽ��Ҫ���ǻ������ݵ��߳�
			while (taskSize_ == 0 && isPoolRunning_)
			{
				if (poolMode_ == PoolMode::MODE_CACHED) //catchģʽ��Ҫ���ǻ������ݵ��߳�
				{
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lck, std::chrono::seconds(1))) //��ʱ����
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count();
						if (dur >= THREAD_MAX_IDLE_TIME &&
							curThreadSize_ > initThreadSize_)
						{
							threadMap_.erase(threadid);
							curThreadSize_--;
							idleThreadSize_--;
							std::cout << "threadid: " << std::this_thread::get_id() << "exit"
								<< std::endl;
							return;
						}
					}
				}
				else// �̵߳ȴ�
				{
					notEmpty_.wait(lck);
				}
			}

			//�̳߳عر�
			if (isPoolRunning_ == false) break; 
			//�����������
			idleThreadSize_--;
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			//�����������֪ͨ�����̼߳���ȡ������
			if (taskSize_ > 0) notEmpty_.notify_all();
			//��ִ��֮ǰ֪ͨ�������Ѿ�ȡ��һ�������ˣ�������߳�ִ�л�����ס������֪ͨ����
			notFull_.notify_all();
		}
		if (task != nullptr)
		{
			//task->run();
			task->exec();
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
	threadMap_.erase(threadid);
	std::cout << "threadid: " << std::this_thread::get_id() << "exit" << std::endl;
	exitCond_.notify_all();
	return;
}
// ���pool������״̬
bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}
/*---------Thread::----------*/
int Thread::generateId_ = 0;
Thread::Thread(func fun)
	:func_(fun)
	,threadId_(generateId_++)
{

}
Thread::~Thread()
{

}
//�����߳�
void Thread::start()
{
	std::thread t(func_, threadId_);
	t.detach();
}
//��ȡ��ǰ�߳�id
int Thread::getId()
{
	return threadId_;
}
/*--------Task::----------------*/
Task::Task()
	:result_(nullptr)
{

}
void Task::setResult(Result* res)
{
	result_ = res;
}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());
	}
}