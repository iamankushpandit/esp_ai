#ifndef TLIB_LINEAR_SHARED_H_
#define TLIB_LINEAR_SHARED_H_

#include <cstdint>

#include <freertos/FreeRTOS.h>

namespace tlib::ops::shared
{

    /**
     * Supported operators that allow shared execution
     */
    enum class OperatorID : uint8_t
    {
        LINEAR_B_RELU_LSHIFT,
        LINEAR_B_RELU_RSHIFT,
        LINEAR_RELU_LSHIFT,
        LINEAR_RELU_RSHIFT,
        LINEAR_DEQ,
        LINEAR_B_DEQ,
        LINEAR
    };

    /**
     * Container that holds all necessary information for
     * a shared operatior execution.
     */
    struct OperatorExecutionRequest
    {
        OperatorID id; /*!< ID of operation to be executed */
        const void *a; /*!< First input data (left input)*/
        const void *b; /*!< Second input data (right input) */
        const void *c; /*!< Optional third input data (bias) */
        void *y;       /*!< Result data */
        uint32_t k;    /*!< Inner dimension*/
        uint32_t n;    /*!< Left outer dimension */
        uint32_t m;    /*!< Right outer dimension */
        uint8_t shift; /*!< Optional result shift */
    };

    /**
     * Check if shared execution is currently enabled. Shared execution
     * is by default disabled. Note that it is independent of the
     * availability of a co worker (see IsSharedExecutionAvailable).
     * 
     * @return  True, if shared execution is enabled, false otherwise
     */
    bool IsSharedExecutionEnabled(void);

    /**
     * Enables shared execution.
     */
    void EnableSharedExecution(void);

    /**
     * Disables shared execution.
     */
    void DisableSharedExecution(void);

    /**
     * Check if a co worker is registered.
     * 
     * @return  True, if a co worker is registered, false otherwise
     */
    bool IsCoWorkerRegistered(void);

    /**
     * Check if shared execution is currently available. Shared execution
     * is only available if a co worker is registered and shared execution
     * is enabled.
     *
     * @return  True, if shared execution is enabled and a co worker is 
     *          registered, false otherwise
     */
    bool IsSharedExecutionAvailable(void);

    /**
     * Check if the current task is registered as co worker.
     *
     * @return  True, if the current task is registered as co worker, false otherwise
     */
    bool IsCoWorker(void);

    /**
     * Check if the current task is not registered as co worker.
     *
     * @return  True, if the current task is not registered as co worker, false otherwise
     */
    bool IsMainWorker(void);

    /**
     * Registers the current task as co worker.
     */
    void RegisterCurrentTask(void);

    /**
     * Unregisters the current task as co worker.
     */
    void UnregisterCurrentTask(void);

    /**
     * Sends a shared operator execution request to the co worker.
     *
     * @param   request The execution request to send
     * 
     * @return  True, if the request could be sent to the co worker, false otherwise.
     */
    bool Execute(const OperatorExecutionRequest request);

    /**
     * Blocks the current task until the other worker is finished if shared execution is
     * avaiable. Otherwise, does nothing and just returns.
     */
    void Synchronize(void);

} // tlib::ops::shared

#endif