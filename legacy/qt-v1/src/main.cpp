/**
 * @file main.cpp
 * @brief 应用程序入口
 *
 * 所有初始化逻辑封装在 Application 类中。
 */

#include "core/Application.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    return app.run();
}
