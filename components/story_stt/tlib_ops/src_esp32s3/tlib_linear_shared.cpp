#include "tlib_linear_shared.h"

#include <cassert>
#include <print>

namespace tlib::ops::shared
{

    const uint32_t main_worker_event_bit{1 << 0};
    const uint32_t co_worker_event_bit{1 << 1};
    static bool enabled_{false};
    static OperatorExecutionRequest current_request_{};
    static TaskHandle_t co_worker_task_handle_{nullptr};
    static EventGroupHandle_t task_sync_event_group_{nullptr};

    bool IsSharedExecutionEnabled(void)
    {
        return enabled_;
    }

    void EnableSharedExecution(void)
    {
        enabled_ = true;
    }

    void DisableSharedExecution(void)
    {
        enabled_ = false;
    }

    bool IsCoWorkerRegistered(void) {
        return co_worker_task_handle_ != nullptr;
    }

    bool IsSharedExecutionAvailable(void)
    {
        return IsCoWorkerRegistered() && IsSharedExecutionEnabled();
    }

    bool IsCoWorker(void)
    {
        return co_worker_task_handle_ == xTaskGetCurrentTaskHandle();
    }

    bool IsMainWorker(void)
    {
        return !IsCoWorker();
    }

    void RegisterCurrentTask(void)
    {
        assert(co_worker_task_handle_ == nullptr);
        co_worker_task_handle_ = xTaskGetCurrentTaskHandle();
        task_sync_event_group_ = xEventGroupCreate();
    }

    void UnregisterCurrentTask(void)
    {
        co_worker_task_handle_ = nullptr;
        // story: the co-worker is created/destroyed per phase; don't leak the group.
        if (task_sync_event_group_) {
            vEventGroupDelete(task_sync_event_group_);
            task_sync_event_group_ = nullptr;
        }
    }

    bool Execute(const OperatorExecutionRequest request)
    {
        assert(IsSharedExecutionAvailable());
        assert(IsMainWorker());

        current_request_ = request;
        if (xTaskNotify(co_worker_task_handle_, reinterpret_cast<uint32_t>(&current_request_), eSetValueWithoutOverwrite) == pdFALSE)
        {
            std::println("Warning: Failed to notify co worker");
            return false;
        }
        return true;
    }

    void Synchronize(void)
    {
        if (!IsSharedExecutionAvailable())
        {
            return;
        }
        if (IsCoWorker())
        {
            xEventGroupSetBits(task_sync_event_group_, co_worker_event_bit);
            xEventGroupWaitBits(task_sync_event_group_, main_worker_event_bit, pdTRUE, pdTRUE, portMAX_DELAY);
        }
        else
        {
            xEventGroupSetBits(task_sync_event_group_, main_worker_event_bit);
            xEventGroupWaitBits(task_sync_event_group_, co_worker_event_bit, pdTRUE, pdTRUE, portMAX_DELAY);
        }
    }

} // tlib::ops::shared