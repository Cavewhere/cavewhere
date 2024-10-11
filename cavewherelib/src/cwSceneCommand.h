/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWSCENECOMMAND_H
#define CWSCENECOMMAND_H

/**
 * @brief The cwSceneCommand class
 *
 * Scene commands are subclass and added to a cwScene. Once added the
 * cwScene take responsiblity for command.  All commands are executed
 * in the rendering thread and are useful for initilizing, updating,
 * and deleting opengl resources.
 */
class cwSceneCommand
{
public:
    cwSceneCommand();
    virtual ~cwSceneCommand();

    virtual void excute() = 0;

};

#endif // CWSCENECOMMAND_H
