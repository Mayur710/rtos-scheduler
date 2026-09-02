#ifndef task_h
#define task_h
#include <string>


/*task state will mirror the RTOS task state machine 
READY - task is eligible to run 
RUNNING - task is currently executing 
BLOCKED - task is waiting for a resource or event
DONE - task has completed execution */  

enum class TaskState{//create own custom data type to represent the state of a task
    READY, 
    RUNNING,
    BLOCKED,
    DONE
};

class Task{
public :
    Task(int id , int priority , const std::string& name);
    virtual ~Task() = default; //if child classis destroyed the program will properly trigger that child's cleanup process, preventing memory leaks.
    //each concrete task will implement its own run method called once per scheduler tick
    virtual void run() = 0;

    int getId() const;
    int getPriority() const;
    const std::string& getName() const;
    TaskState getState() const;

    void setState(TaskState newState);

protected:
    int id_;
    int priority_;
    std::string name_;
    TaskState state_;

};
#endif