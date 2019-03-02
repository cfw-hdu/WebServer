//
// Created by marvinle on 2019/2/26 3:53 PM.
//

#include <sys/time.h>
#include <unistd.h>

#include "../include/Timer.h"
#include "../include/Epoll.h"



size_t TimerNode::current_msec = 0; // 当前时间

const size_t TimerManager::DEFAULT_TIME_OUT = 5000; // 5s



TimerNode::TimerNode(std::shared_ptr<HttpData> httpData, size_t timeout) : deleted_(false), httpData_(httpData) {
        current_time();
        expiredTime_ = current_msec + timeout;
}

TimerNode::~TimerNode() {
    //FIXME 析构关闭资源的时候，要讲httpDataMap中的引用,否则资源无法关闭，后期可改进为httpDataMap存储 weak_ptr<HttpData>
    auto it = Epoll::httpDataMap.find(httpData_->clientSocket_->fd);
    if (it != Epoll::httpDataMap.end()) {
        Epoll::httpDataMap.erase(it);
    }
}

void inline TimerNode::current_time() {
    struct timeval cur;
    gettimeofday(&cur, NULL);
    current_msec = (cur.tv_sec * 1000) + (cur.tv_usec / 1000);
}


void TimerNode::deleted() {
    // 删除采用标记删除， 并及时析构HttpData，以关闭描述符
    httpData_.reset();
    deleted_ = true;
}


void TimerManager::addTimer(std::shared_ptr<HttpData> httpData, size_t timeout) {
    Shared_TimerNode timerNode(new TimerNode(httpData, timeout));
    {
        MutexLockGuard guard(lock_);
        TimerQueue.push(timerNode);
        // 将TimerNode和HttpData关联起来
        httpData->setTimer(timerNode);
    }
}

void TimerManager::handle_expired_event() {
    MutexLockGuard guard(lock_);
    // 更新当前时间
    TimerNode::current_time();
    while(!TimerQueue.empty()) {
        Shared_TimerNode timerNode = TimerQueue.top();
        if (timerNode->isDeleted()) {
            // 删除节点
            TimerQueue.pop();
        } else if (timerNode->isExpire()) {
            // 过期 删除
            TimerQueue.pop();
        } else {
            break;
        }
    }
}


