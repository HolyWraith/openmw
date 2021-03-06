#include "workqueue.hpp"

#include <iostream>

namespace SceneUtil
{

void WorkItem::waitTillDone()
{
    if (mDone > 0)
        return;

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mMutex);
    while (mDone == 0)
    {
        mCondition.wait(&mMutex);
    }
}

void WorkItem::signalDone()
{
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mMutex);
        mDone.exchange(1);
    }
    mCondition.broadcast();
}

WorkItem::WorkItem()
{
}

WorkItem::~WorkItem()
{
}

bool WorkItem::isDone() const
{
    return (mDone > 0);
}

WorkQueue::WorkQueue(int workerThreads)
    : mIsReleased(false)
{
    for (int i=0; i<workerThreads; ++i)
    {
        WorkThread* thread = new WorkThread(this);
        mThreads.push_back(thread);
        thread->startThread();
    }
}

WorkQueue::~WorkQueue()
{
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mMutex);
        while (!mQueue.empty())
            mQueue.pop();
        mIsReleased = true;
        mCondition.broadcast();
    }

    for (unsigned int i=0; i<mThreads.size(); ++i)
    {
        mThreads[i]->join();
        delete mThreads[i];
    }
}

void WorkQueue::addWorkItem(osg::ref_ptr<WorkItem> item)
{
    if (item->isDone())
    {
        std::cerr << "warning, trying to add a work item that is already completed" << std::endl;
        return;
    }

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mMutex);
    mQueue.push(item);
    mCondition.signal();
}

osg::ref_ptr<WorkItem> WorkQueue::removeWorkItem()
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mMutex);
    while (mQueue.empty() && !mIsReleased)
    {
        mCondition.wait(&mMutex);
    }
    if (mQueue.size())
    {
        osg::ref_ptr<WorkItem> item = mQueue.front();
        mQueue.pop();
        return item;
    }
    else
        return NULL;
}

WorkThread::WorkThread(WorkQueue *workQueue)
    : mWorkQueue(workQueue)
{
}

void WorkThread::run()
{
    while (true)
    {
        osg::ref_ptr<WorkItem> item = mWorkQueue->removeWorkItem();
        if (!item)
            return;
        item->doWork();
        item->signalDone();
    }
}

}
