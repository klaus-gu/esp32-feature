// driver.h —— drivers 层：硬件驱动统一接口
//
// 约定：每个硬件驱动实现 Driver，并在自己的初始化函数里 drivers::add(this)。
// core 与 modules 不直接 include 任何具体驱动头文件，只调用 beginAll()/pollAll()。
// 这样新增 OLED / 传感器 / 舵机时，主循环和 core 层都不需要修改。

#pragma once

namespace drivers {

class Driver {
 public:
  virtual ~Driver() = default;

  // 驱动名称（日志用），必须可读，例如 "OLED"、"BME280"
  virtual const char* name() const = 0;

  // 初始化硬件；失败返回 false。失败不得让系统卡死，由调用方记录并跳过。
  virtual bool begin() = 0;

  // 周期任务，必须非阻塞（禁止 delay 等待硬件）
  virtual void poll() = 0;

  // 可选：供调试输出当前状态
  virtual void printStatus() {}
};

// 注册驱动（通常在 foundation.ino 的 setup 里完成）
void add(Driver* driver);

// 依次初始化所有驱动；返回失败数量
int beginAll();

// 依次执行所有驱动的周期任务
void pollAll();

// 依次打印所有驱动状态
void printStatusAll();

}  // namespace drivers
