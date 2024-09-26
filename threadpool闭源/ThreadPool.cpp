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
// 设置工作模式
void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}
// 设置任务队列最大任务数量
void ThreadPool::setTaskQue(int maxsize)
{
	maxTaskSize_ = maxsize;
}
// 提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lck(taskQueMtx_);

	//判断任务队列数量  满=》等待  不满=》提交任务  
	//可以设置一个输出 如果满了每1s打印一次  
	if (!notFull_.wait_for(lck, std::chrono::seconds(1)
		, [&]()->bool {return taskSize_ < maxTaskSize_; }))
	{
		std::cout << "taskQue is full, wait ...." << std::endl;
		return Result(sp, false);
	}

	taskQue_.emplace(sp);
	taskSize_++;
	notEmpty_.notify_all();
	
	//如果是CATCH模式，则判断当前任务数量是否大于空闲线程数，当前线程数量是否小于最大线程数
	//保证分配尽量多的线程去处理任务
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
// 开启线程池
void ThreadPool::start(int initThreadSize)
{
	//初始化线程数量
	isPoolRunning_ = true;
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;//当前线程数量
	// 创建线程对象
	for (int i = 0; i < initThreadSize_; ++i)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc
												, this, std::placeholders::_1));
		int id = ptr->getId();
		threadMap_.emplace(id, std::move(ptr));
	}

	// 初始化线程列表
	for (int i = 0; i < initThreadSize_; ++i)
	{
		threadMap_[i]->start();
		idleThreadSize_++;//空闲线程数量
	}

}
// 线程处理函数
/*
catch模式下：不能让扩容的线程一直执行，浪费不必要的资源，如果在一分钟内没有新任务，就销毁！！！
*/
void ThreadPool::threadFunc(int threadid)
{
	std::cout << "threadID:" << std::this_thread::get_id() << std::endl;
	auto lastTime = std::chrono::high_resolution_clock().now();
	//保证所有线程都在工作函数中
	while (isPoolRunning_)
	{
		std::shared_ptr<Task> task = nullptr;
		{
			//获取锁
			std::unique_lock<std::mutex> lck(taskQueMtx_);

			//如果任务队列空，而且是catch模式就要考虑回收扩容的线程
			while (taskSize_ == 0 && isPoolRunning_)
			{
				if (poolMode_ == PoolMode::MODE_CACHED) //catch模式就要考虑回收扩容的线程
				{
					if (std::cv_status::timeout ==
						notEmpty_.wait_for(lck, std::chrono::seconds(1))) //超时返回
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
				else// 线程等待
				{
					notEmpty_.wait(lck);
				}
			}

			//线程池关闭
			if (isPoolRunning_ == false) break; 
			//更新任务队列
			idleThreadSize_--;
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			//如果还有任务，通知其他线程继续取出任务
			if (taskSize_ > 0) notEmpty_.notify_all();
			//在执行之前通知生产者已经取出一个任务了，否则该线程执行会阻塞住，半天通知不到
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
// 检查pool的运行状态
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
//启动线程
void Thread::start()
{
	std::thread t(func_, threadId_);
	t.detach();
}
//获取当前线程id
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