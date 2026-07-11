#include <Arduino.h>
#include <EEPROM.h>
#include <BMI160Gen.h>
#include <math.h>
#include "DengFOC.h"
#include "face_tracking_policy.h"
#include "lowpass_filter.h"
#include "pid.h"

//适配v3p和v4板子   0：V3P  1：V4
#define DengFOC 1 

// 定义EEPROM模拟存储的大小（字节），ESP32建议不超过4096字节
#define EEPROM_SIZE 32
// 定义存储数据的起始地址
#define M0_Initialize_Align 0
#define M1_Initialize_Align 4
#define M0_Encoder_DIR 8
#define M1_Encoder_DIR 12

//设置报警电压
#define UNDERVOLTAGE_THRES 11.1

// BMI160 IMU has been removed from the Grind Buddy FOC board.
// Keep this off so the gimbal can run from AS5600 encoder feedback only.
#define ENABLE_IMU_CONTROL 0

// 判断是否已经写入过EEPROM 1：需要写入 0：已经写入
#define Calibration_Flag 0     

// 驱动效果测试菜单：上电默认STOP，通过串口 MODE/RUN/STOP/T/S/? 切换测试
#define DRIVE_TEST_MENU 0
#define DRIVE_TEST_DEFAULT_MODE 5
#define DRIVE_TEST_DEFAULT_TARGET_DEG 30.0f
#define DRIVE_TEST_DEFAULT_SPEED 1.0f
#define DRIVE_TEST_DEFAULT_UQ 0.8f
#define DRIVE_TEST_DEBUG_INTERVAL_MS 250UL

// M1开环直驱测试：1开启，0关闭并恢复云台闭环控制
#define DIRECT_DRIVE_TEST 0
#define DIRECT_DRIVE_UQ 1.0f
#define DIRECT_DRIVE_SPEED 0.015f

// M1编码器位置锁定测试：1锁住上电时M1位置，0使用原水平轴陀螺仪控制
#define M1_ENCODER_LOCK_TEST 1

// 串口目标角度控制：T30,-20 同时设置M0/M1，M0 30或M1 -20单轴设置，H回零
#define SERIAL_TARGET_CONTROL 1
#define SERIAL_MOVE_DURATION_MS 1600UL
#define SERIAL_DEBUG_OUTPUT 1
#define SERIAL_DEBUG_INTERVAL_MS 250UL
#define FOC_COMMAND_UART_BAUD_RATE 115200
#define FOC_COMMAND_UART_RX_PIN 16
#define FOC_COMMAND_UART_TX_PIN 17
#define VISION_M0_MIN_DEG -90.0f
#define VISION_M0_MAX_DEG 90.0f
#define VISION_M1_MIN_DEG -180.0f
#define VISION_M1_MAX_DEG 180.0f
#define VISION_TARGET_DEADBAND_DEG 0.3f

// 目标角度演示：两轴围绕上电位置平滑摆动
#define TARGET_ANGLE_DEMO 0
#define TARGET_SWING_AMPLITUDE_DEG 90.0f
#define TARGET_SWING_PERIOD_MS 4000UL
#define TARGET_MAX_VELOCITY_DEG_S 160.0f
#define TARGET_MAX_ACCEL_DEG_S2 520.0f
#define TARGET_POSITION_KP 0.08f
#define TARGET_VELOCITY_KD 0.0f
#define TARGET_VELOCITY_FF 0.35f
#define TARGET_STATIC_FF 0.34f
#define TARGET_STATIC_FF_BLEND_VEL 0.45f
#define TARGET_TORQUE_LIMIT 6.0f
#define CASCADE_POSITION_P 8.0f
#define CASCADE_VELOCITY_LIMIT 5.0f
#define CASCADE_VEL_P 0.04f
#define CASCADE_VEL_I 0.35f
#define CASCADE_VEL_LIMIT 3.0f

LowPassFilter Y_Flt = LowPassFilter(0.02); //垂直电机滤波

PIDController Y_loop = PIDController(0.08, 0.0, 0.0, 100000, 6);//垂直电机PID，只用P减少静止抖动

LowPassFilter M1_Lock_Flt = LowPassFilter(0.02); //水平电机锁定测试滤波，先复制M0手感
PIDController M1_lock_loop = PIDController(0.08, 0.0, 0.0, 100000, 6);//水平电机锁定测试PID，只用P减少静止抖动

LowPassFilter Z_Flt = LowPassFilter(0.09); //水平电机滤波
LowPassFilter Z_Flt_Damping = LowPassFilter(0.1); //水平电机阻尼


PIDController Z_loop = PIDController(0.030, 0.001, 0.005, 100000, 6);//水平电机PID

int ax, ay, az;       // 加速度计原始值 
int gx, gy, gz;       // 陀螺仪原始值
int bx,by,bz;         // 陀螺仪方向匹配
int cx,cy,cz;
float accX, accY, accZ;       // 换算后的加速度值（g单位）
float gyroX, gyroY,gyroZ;     // 换算后的角速度值（dps单位）
float angleAccX, angleAccY;   // 加速度计解算的角度
float interval;               // 时间间隔（秒）                                                             
unsigned long preInterval = 0; // 时间戳
float gyroXoffset = 0.0f, gyroYoffset = 0.0f,gyroZoffset = 0.0f;        // 陀螺仪偏移值（校准后赋值）
float last_angleX = 0.0f, last_angleY = 0.0f,last_angleZ = 0.0f;     // 获取的角度值


const float DEAD_ZONE = 0.05f;  // 陀螺仪死区，过滤微小噪声

float filteredGyroX = 0.0f;     // 滤波后的角速度
float filteredGyroY = 0.0f;     // 滤波后的角速度
float filteredGyroZ = 0.0f;     // 滤波后的角速度

float Torget_0 = 0;             //垂直电机目标力矩
float Torget_1 = 0;             //水平电机目标力矩
float angle1_error_0 = 0;       //垂直电机角度偏差
float angle1_error_1 = 0;       //水平电机角度偏差
float angle1_error_1_begin = 0; //判断是否静止了
int Acc = 0;                    //累加多次静止

float now = 0;                  //记录垂直电机编码器角度与陀螺仪角度偏差值
float M0_home_angle = 0;        //垂直电机上电基准角度
float M1_home_angle = 0;        //水平电机上电基准角度
float M0_target_angle = 0;      //垂直电机目标角度
float M1_target_angle = 0;      //水平电机目标角度
float M0_target_velocity = 0;   //垂直电机目标速度
float M1_target_velocity = 0;   //水平电机目标速度
unsigned long target_demo_start_ms = 0;
unsigned long target_demo_last_us = 0;
unsigned long target_demo_segment_start_us = 0;
float target_demo_offset = 0.0f;
float target_demo_velocity = 0.0f;
float target_demo_segment_start_offset = 0.0f;
float target_demo_goal_offset = 0.0f;
float M0_move_start_angle = 0.0f;
float M1_move_start_angle = 0.0f;
float M0_move_goal_angle = 0.0f;
float M1_move_goal_angle = 0.0f;
float M0_move_distance = 0.0f;
float M1_move_distance = 0.0f;
unsigned long target_move_start_us = 0;
unsigned long target_move_duration_us = SERIAL_MOVE_DURATION_MS * 1000UL;
bool target_move_active = false;
unsigned long serial_debug_last_ms = 0;

int drive_test_mode = DRIVE_TEST_DEFAULT_MODE;
bool drive_test_running = false;
float drive_test_target_deg_m0 = DRIVE_TEST_DEFAULT_TARGET_DEG;
float drive_test_target_deg_m1 = -DRIVE_TEST_DEFAULT_TARGET_DEG;
float drive_test_speed = DRIVE_TEST_DEFAULT_SPEED;
float drive_test_uq = DRIVE_TEST_DEFAULT_UQ;
float drive_test_open_angle = 0.0f;
unsigned long drive_test_debug_last_ms = 0;

float M0_zero_electric_angle = 0;   //零电角度
float M1_zero_electric_angle = 0;

int M0_Dir = 0;               //自检极性
int M1_Dir = 0;

float direct_drive_angle = 0.0f;

//获取电源电压
float GetVinVolt();

//检测电源电压
void CheckVinVolt();

//EEPROM存储FLOAT
void WriteFloatToEEPROM(int addr, float value);
void WriteIntToEEPROM(int addr, int value);

//EEPROM读取FLOAT
float ReadFloatToEEPROM(int addr);
int ReadIntToEEPROM(int addr);

//陀螺仪初始化
void Bmi160Init();

// 陀螺仪手动校准
void CalibrateGyro();

//获取陀螺仪角度
void GetAngle();

float TargetStaticFeedForward(float target_velocity);
void UpdateTargetAngleDemo();
void MoveTargetCascade();
void ReadSerialTargetCommand();
void ReadSerialTargetCommandFrom(Stream &input, String &command);
bool ParseFloatStrict(String text, float &value);
bool ApplyAxisTarget(bool set_axis, float requested_relative_deg, float min_deg, float max_deg, float home_angle, float &goal_angle);
void StartTargetMove(bool set_m0, float m0_relative_deg, bool set_m1, float m1_relative_deg, unsigned long duration_ms);
void ApplyFaceTrackingCommand(float center_x, float center_y, unsigned long duration_ms);
void UpdateSerialTargetMotion();
void PrintSerialControlHelp();
void PrintSerialTargetStatus();
void PrintSerialDebug();
void ReadDriveTestCommand();
void RunDriveTestMode();
void StopDriveTestOutput();
void PrintDriveTestHelp();
void PrintDriveTestStatus();
void PrintDriveTestDebug();
const char* DriveTestModeName(int mode);
float ClampFloat(float value, float min_value, float max_value);
float SmoothStep5(float progress);
float SmoothStep5Derivative(float progress);

void setup() {
  // 初始化串口，用于打印调试信息
  Serial.begin(115200);
  // 等待串口初始化完成
  while (!Serial) {
    delay(10);
  }
  Serial2.begin(
      FOC_COMMAND_UART_BAUD_RATE,
      SERIAL_8N1,
      FOC_COMMAND_UART_RX_PIN,
      FOC_COMMAND_UART_TX_PIN);

  if(DIRECT_DRIVE_TEST)
  {
    CheckVinVolt();     // 仍然要求电机电源电压正常，避免欠压直驱
    DFOC_Vbus(12.6);
    DFOC_enable();
    Serial.println("M1开环直驱测试模式：只驱动M1慢速旋转");
    return;
  }

  CheckVinVolt();   //电压检测
  DFOC_Vbus(12.6);   //设定驱动器供电电压

  #if ENABLE_IMU_CONTROL
    Bmi160Init();
    CalibrateGyro();
  #else
    Serial.println("IMU disabled; using encoder-only target control");
  #endif
  preInterval = millis();
    
  DFOC_enable();

  // 初始化EEPROM，设置模拟存储大小
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("EEPROM初始化失败！");
    while (1) { // 初始化失败则卡死，提示错误
      delay(1000);
    }
  }

  if(Calibration_Flag)                                      //判断是否需要记录0电角度
  {
    M0_zero_electric_angle = DFOC_M0_alignSensor(7); //M0电机，极对数和dir
    M1_zero_electric_angle = DFOC_M1_alignSensor(7); //M1电机，极对数和dir
    M0_Dir = Get_M0_Dir();
    M1_Dir = Get_M1_Dir();

    Serial.println("正在存入电角度");
    WriteFloatToEEPROM(M0_Initialize_Align, M0_zero_electric_angle);
    delay(10);
    WriteFloatToEEPROM(M1_Initialize_Align, M1_zero_electric_angle);
    delay(10);
    WriteIntToEEPROM(M0_Encoder_DIR, M0_Dir);
    delay(10);
    WriteIntToEEPROM(M1_Encoder_DIR, M1_Dir);
    
    Serial.println("存入完成");
    Serial.println("请进一步标定陀螺仪");
    while(1);
  }
  else
  {
    Serial.println("正在读取零电角度。。");
    M0_zero_electric_angle = ReadFloatToEEPROM(M0_Initialize_Align);
    delay(10);
    M1_zero_electric_angle = ReadFloatToEEPROM(M1_Initialize_Align);
    delay(10);
    M0_Dir = ReadIntToEEPROM(M0_Encoder_DIR);
    delay(10);
    M1_Dir = ReadIntToEEPROM(M1_Encoder_DIR);

    DFOC_M0_Set_alignSensor(7,M0_Dir,M0_zero_electric_angle);
    DFOC_M1_Set_alignSensor(7,M1_Dir,M1_zero_electric_angle);
    DFOC_M0_SET_ANGLE_PID(1.0f, 0.0f, 0.0f, 100000, 30.0f);
    DFOC_M1_SET_ANGLE_PID(1.0f, 0.0f, 0.0f, 100000, 30.0f);
    DFOC_M0_SET_VEL_PID(CASCADE_VEL_P, CASCADE_VEL_I, 0.0f, 100000, CASCADE_VEL_LIMIT);
    DFOC_M1_SET_VEL_PID(CASCADE_VEL_P, CASCADE_VEL_I, 0.0f, 100000, CASCADE_VEL_LIMIT);
    Serial.print("DIR为：");
    Serial.print(M0_Dir);
    Serial.print(",");
    Serial.println(M1_Dir);

    Serial.println("读取零电角度完成");
    Serial.print("零电角度为：");
    Serial.print(M0_zero_electric_angle);  
    Serial.print(",");
    Serial.println(M1_zero_electric_angle);
  }
  
  EEPROM.end();
  #if ENABLE_IMU_CONTROL
    now = (DFOC_M0_Angle()*180/PI)- M0_Dir * last_angleY - M0_Dir * 90;   //记录编码器和陀螺仪的差值 -90是因为初始标定的偏差
  #else
    now = 0.0f;
  #endif
  M0_home_angle = DFOC_M0_Angle();
  M1_home_angle = DFOC_M1_Angle();
  M0_target_angle = M0_home_angle;
  M1_target_angle = M1_home_angle;
  M0_move_start_angle = M0_home_angle;
  M1_move_start_angle = M1_home_angle;
  M0_move_goal_angle = M0_home_angle;
  M1_move_goal_angle = M1_home_angle;
  target_demo_start_ms = millis();
  target_demo_last_us = micros();
  target_demo_segment_start_us = target_demo_last_us;
  target_demo_offset = 0.0f;
  target_demo_velocity = 0.0f;
  target_demo_segment_start_offset = 0.0f;
  target_demo_goal_offset = TARGET_SWING_AMPLITUDE_DEG * PI / 180.0f;
  // now = (DFOC_M0_Angle()*180/PI)- last_angleY;   //记录编码器和陀螺仪的差值 90是因为初始标定的偏差
  #if ENABLE_IMU_CONTROL
    GetAngle();
  #endif
  Serial.println("初始化完成！");
  if(DRIVE_TEST_MENU)
  {
    PrintDriveTestHelp();
    PrintDriveTestStatus();
  }
  if(SERIAL_TARGET_CONTROL)
  {
    PrintSerialControlHelp();
  }
  delay(50);
}

void loop() {
  if(DIRECT_DRIVE_TEST)
  {
    M0_setTorque(0.0f, 0.0f);
    M1_setTorque(DIRECT_DRIVE_UQ, direct_drive_angle);
    direct_drive_angle += DIRECT_DRIVE_SPEED;
    if(direct_drive_angle > 2 * PI)
    {
      direct_drive_angle -= 2 * PI;
    }
    delay(2);
    return;
  }

  runFOC();
  if(DRIVE_TEST_MENU)
  {
    ReadDriveTestCommand();
    RunDriveTestMode();
    PrintDriveTestDebug();
    return;
  }

  if(SERIAL_TARGET_CONTROL)
  {
    ReadSerialTargetCommand();
    UpdateSerialTargetMotion();
    DFOC_M0_set_Velocity_Angle(M0_target_angle);
    DFOC_M1_set_Velocity_Angle(M1_target_angle);
    PrintSerialDebug();
    return;
  }

  if(!TARGET_ANGLE_DEMO)
  {
    GetAngle();
  }
  if(TARGET_ANGLE_DEMO)
  {
    UpdateTargetAngleDemo();
    MoveTargetCascade();
    return;
  }

  // Serial.print(last_angleX);
  // Serial.print(",");
  // Serial.print(last_angleY);
  // Serial.print(",");
  // Serial.println(last_angleZ);
  // /********************************************垂直 *********************************************/
  if(TARGET_ANGLE_DEMO)
  {
    angle1_error_0 = (M0_target_angle - DFOC_M0_Angle()) * 180 / PI;
    float torque_0 = TARGET_POSITION_KP * angle1_error_0 + TARGET_VELOCITY_FF * M0_target_velocity + TargetStaticFeedForward(M0_target_velocity);
    Torget_0 = Y_Flt(constrain(torque_0, -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
  }
  else
  {
    angle1_error_0 = M0_Dir * last_angleY - 0  + now;    //陀螺仪角度映射到目标位置
    if(fabs(angle1_error_0) < 0.6f)
    {
      Torget_0 = 0.0f;
    }
    else{
      Torget_0 = Y_Flt(Y_loop(angle1_error_0-(DFOC_M0_Angle()*180/PI)));
    }
  }

    
  // Serial.print(last_angleY);
  // Serial.print(",");
  // Serial.print(now);
  // Serial.print(",");
  // Serial.print(angle1_error_0);
  // Serial.print(",");
  // Serial.print(DFOC_M0_Angle()*180/PI);
  // Serial.print(",");
  // Serial.println(angle1_error_0-(DFOC_M0_Angle()*180/PI));

  /********************************************水平 ********************************************/
  if(M1_ENCODER_LOCK_TEST)
  {
    angle1_error_1 = (M1_target_angle - DFOC_M1_Angle()) * 180 / PI;
    if(TARGET_ANGLE_DEMO)
    {
      float torque_1 = TARGET_POSITION_KP * angle1_error_1 + TARGET_VELOCITY_FF * M1_target_velocity + TargetStaticFeedForward(M1_target_velocity);
      Torget_1 = M1_Lock_Flt(constrain(torque_1, -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
    }
    else
    {
      if(fabs(angle1_error_1) < 0.6f)
      {
        Torget_1 = 0.0f;
      }
      else
      {
        Torget_1 = M1_Lock_Flt(M1_lock_loop(angle1_error_1));
      }
    }
  }
  else
  {
    angle1_error_1 = last_angleZ - 0 ;
    if(fabs(angle1_error_1_begin - angle1_error_1) < 0.05f)                       //多次静止，清空I，避免累加
    {
      Acc++;
      if(Acc > 10)
      {
        Acc = 10;
      }
    }
    else
    {
      Acc = 0;
    }

    if(Acc == 10)
    {
      Z_loop.I = 0.0;
      Torget_1 = Z_Flt(Z_loop(angle1_error_1));
      Torget_1 = 0.0f;
    }
    else{
      Z_loop.I = 0.001;
      if(fabs(angle1_error_1) < 0.3)
      {
        Torget_1 = 0.0f;
      }
      else
      {
        Torget_1 = Z_Flt(Z_loop(angle1_error_1));
      }
    }

    angle1_error_1_begin =  angle1_error_1;
  }

  DFOC_M0_setTorque(Torget_0);
  if(M1_ENCODER_LOCK_TEST)
  {
    DFOC_M1_setTorque(Torget_1);
  }
  else
  {
    DFOC_M1_setTorque((M1_Dir * (Torget_1 + 2.9 * sin(last_angleY * PI / 180)) + (Z_Flt_Damping(DFOC_M1_Angle()) - DFOC_M1_Angle())));                   //前馈增益+角度阻尼
  }
  // Serial.println((M1_Dir * (Torget_1 + 2.9 * sin(last_angleY * PI / 180)) + (Z_Flt_Damping(DFOC_M1_Angle()) - DFOC_M1_Angle())));
}

float TargetStaticFeedForward(float target_velocity)
{
  if(fabs(target_velocity) < 0.001f)
  {
    return 0.0f;
  }
  return TARGET_STATIC_FF * target_velocity / (fabs(target_velocity) + TARGET_STATIC_FF_BLEND_VEL);
}

void UpdateTargetAngleDemo()
{
  unsigned long now_us = micros();
  const float amplitude = TARGET_SWING_AMPLITUDE_DEG * PI / 180.0f;
  const unsigned long segment_duration_us = (TARGET_SWING_PERIOD_MS * 1000UL) / 2UL;
  float progress = (float)(now_us - target_demo_segment_start_us) / (float)segment_duration_us;
  if(progress >= 1.0f)
  {
    target_demo_offset = target_demo_goal_offset;
    target_demo_velocity = 0.0f;
    target_demo_segment_start_offset = target_demo_goal_offset;
    target_demo_goal_offset = -target_demo_goal_offset;
    target_demo_segment_start_us = now_us;
    progress = 0.0f;
  }

  progress = ClampFloat(progress, 0.0f, 1.0f);
  const float distance = target_demo_goal_offset - target_demo_segment_start_offset;
  const float duration_s = (float)segment_duration_us * 1e-6f;
  const float smooth = SmoothStep5(progress);
  const float smooth_d = SmoothStep5Derivative(progress);

  target_demo_offset = ClampFloat(target_demo_segment_start_offset + distance * smooth, -amplitude, amplitude);
  target_demo_velocity = distance * smooth_d / duration_s;
  M0_target_angle = M0_home_angle + target_demo_offset;
  M1_target_angle = M1_home_angle - target_demo_offset;
  M0_target_velocity = target_demo_velocity;
  M1_target_velocity = -target_demo_velocity;
}

void MoveTargetCascade()
{
  const float m0_angle_error = M0_target_angle - DFOC_M0_Angle();
  const float m1_angle_error = M1_target_angle - DFOC_M1_Angle();
  const float m0_velocity_target = ClampFloat(M0_target_velocity + CASCADE_POSITION_P * m0_angle_error, -CASCADE_VELOCITY_LIMIT, CASCADE_VELOCITY_LIMIT);
  const float m1_velocity_target = ClampFloat(M1_target_velocity + CASCADE_POSITION_P * m1_angle_error, -CASCADE_VELOCITY_LIMIT, CASCADE_VELOCITY_LIMIT);

  const float m0_torque = DFOC_M0_VEL_PID(m0_velocity_target - DFOC_M0_Velocity()) + TargetStaticFeedForward(m0_velocity_target);
  const float m1_torque = DFOC_M1_VEL_PID(m1_velocity_target - DFOC_M1_Velocity()) + TargetStaticFeedForward(m1_velocity_target);

  DFOC_M0_setTorque(ClampFloat(m0_torque, -CASCADE_VEL_LIMIT, CASCADE_VEL_LIMIT));
  DFOC_M1_setTorque(ClampFloat(m1_torque, -CASCADE_VEL_LIMIT, CASCADE_VEL_LIMIT));
}

void ReadSerialTargetCommand()
{
  static String usb_command;
  static String foc_command;

  ReadSerialTargetCommandFrom(Serial, usb_command);
  ReadSerialTargetCommandFrom(Serial2, foc_command);
}

void ReadSerialTargetCommandFrom(Stream &input, String &command)
{
  while(input.available() > 0)
  {
    char ch = (char)input.read();
    if(ch == '\r')
    {
      continue;
    }
    if(ch != '\n')
    {
      command += ch;
      if(command.length() > 48)
      {
        command = "";
      }
      continue;
    }

    command.trim();
    if(command.length() == 0)
    {
      return;
    }

    String upper = command;
    upper.toUpperCase();
    unsigned long duration_ms = SERIAL_MOVE_DURATION_MS;
    int at_pos = upper.indexOf('@');
    if(at_pos >= 0)
    {
      duration_ms = (unsigned long)upper.substring(at_pos + 1).toInt();
      duration_ms = constrain(duration_ms, 200UL, 10000UL);
      upper = upper.substring(0, at_pos);
      command = command.substring(0, at_pos);
      command.trim();
      upper.trim();
    }

    if(upper == "?" || upper == "HELP")
    {
      PrintSerialControlHelp();
    }
    else if(upper == "S")
    {
      PrintSerialTargetStatus();
    }
    else if(upper == "H")
    {
      StartTargetMove(true, 0.0f, true, 0.0f, duration_ms);
    }
    else if(upper.startsWith("T"))
    {
      String payload = command.substring(1);
      payload.trim();
      int comma_pos = payload.indexOf(',');
      float m0_deg = 0.0f;
      float m1_deg = 0.0f;
      if(comma_pos >= 0)
      {
        if(!ParseFloatStrict(payload.substring(0, comma_pos), m0_deg) || !ParseFloatStrict(payload.substring(comma_pos + 1), m1_deg))
        {
          Serial.println("ERR,invalid target,use T m0,m1");
          command = "";
          continue;
        }
        StartTargetMove(true, m0_deg, true, m1_deg, duration_ms);
      }
      else
      {
        float both_deg = 0.0f;
        if(!ParseFloatStrict(payload, both_deg))
        {
          Serial.println("ERR,invalid target,use T angle");
          command = "";
          continue;
        }
        StartTargetMove(true, both_deg, true, both_deg, duration_ms);
      }
    }
    else if(upper.startsWith("F"))
    {
      String payload = command.substring(1);
      payload.trim();
      int comma_pos = payload.indexOf(',');
      float center_x = 0.0f;
      float center_y = 0.0f;
      if(comma_pos < 0 ||
          !ParseFloatStrict(payload.substring(0, comma_pos), center_x) ||
          !ParseFloatStrict(payload.substring(comma_pos + 1), center_y))
      {
        Serial.println("ERR,invalid face target,use F x,y");
        command = "";
        continue;
      }
      ApplyFaceTrackingCommand(center_x, center_y, duration_ms);
    }
    else if(upper.startsWith("M0"))
    {
      float m0_deg = 0.0f;
      if(!ParseFloatStrict(command.substring(2), m0_deg))
      {
        Serial.println("ERR,invalid target,use M0 angle");
        command = "";
        continue;
      }
      StartTargetMove(true, m0_deg, false, 0.0f, duration_ms);
    }
    else if(upper.startsWith("M1"))
    {
      float m1_deg = 0.0f;
      if(!ParseFloatStrict(command.substring(2), m1_deg))
      {
        Serial.println("ERR,invalid target,use M1 angle");
        command = "";
        continue;
      }
      StartTargetMove(false, 0.0f, true, m1_deg, duration_ms);
    }
    else
    {
      Serial.println("ERR,unknown command,use ? for help");
    }

    command = "";
  }
}

bool ParseFloatStrict(String text, float &value)
{
  text.trim();
  if(text.length() == 0)
  {
    return false;
  }

  char *end_ptr = nullptr;
  value = strtof(text.c_str(), &end_ptr);
  while(end_ptr != nullptr && (*end_ptr == ' ' || *end_ptr == '\t'))
  {
    end_ptr++;
  }
  return end_ptr != text.c_str() && end_ptr != nullptr && *end_ptr == '\0';
}

void ApplyFaceTrackingCommand(float center_x, float center_y, unsigned long duration_ms)
{
  const float current_m0_deg = (M0_move_goal_angle - M0_home_angle) * 180.0f / PI;
  const float current_m1_deg = (M1_move_goal_angle - M1_home_angle) * 180.0f / PI;
  const FaceTrackingTarget target = ApplyFaceTrackingOffset(
      center_x,
      center_y,
      current_m0_deg,
      current_m1_deg);
  if(!target.updated)
  {
    return;
  }
  StartTargetMove(true, target.m0_degrees, true, target.m1_degrees, duration_ms);
}

bool ApplyAxisTarget(bool set_axis, float requested_relative_deg, float min_deg, float max_deg, float home_angle, float &goal_angle)
{
  if(!set_axis)
  {
    return false;
  }

  const float clamped_deg = ClampFloat(requested_relative_deg, min_deg, max_deg);
  const float current_goal_deg = (goal_angle - home_angle) * 180.0f / PI;
  if(fabs(clamped_deg - current_goal_deg) < VISION_TARGET_DEADBAND_DEG)
  {
    return false;
  }

  goal_angle = home_angle + clamped_deg * PI / 180.0f;
  return true;
}

void StartTargetMove(bool set_m0, float m0_relative_deg, bool set_m1, float m1_relative_deg, unsigned long duration_ms)
{
  M0_move_start_angle = DFOC_M0_Angle();
  M1_move_start_angle = DFOC_M1_Angle();

  const bool m0_updated = ApplyAxisTarget(set_m0, m0_relative_deg, VISION_M0_MIN_DEG, VISION_M0_MAX_DEG, M0_home_angle, M0_move_goal_angle);
  const bool m1_updated = ApplyAxisTarget(set_m1, m1_relative_deg, VISION_M1_MIN_DEG, VISION_M1_MAX_DEG, M1_home_angle, M1_move_goal_angle);

  if(!m0_updated && !m1_updated)
  {
    Serial.println("INFO,target ignored by deadband or unchanged");
    PrintSerialTargetStatus();
    return;
  }

  M0_move_distance = M0_move_goal_angle - M0_move_start_angle;
  M1_move_distance = M1_move_goal_angle - M1_move_start_angle;
  target_move_duration_us = constrain(duration_ms, 200UL, 10000UL) * 1000UL;
  target_move_start_us = micros();
  target_move_active = true;
  PrintSerialTargetStatus();
}

void UpdateSerialTargetMotion()
{
  M0_target_angle = M0_move_goal_angle;
  M1_target_angle = M1_move_goal_angle;
  M0_target_velocity = 0.0f;
  M1_target_velocity = 0.0f;
  target_move_active = false;
}

void PrintSerialControlHelp()
{
  Serial.println("CMD,T m0_deg,m1_deg  e.g. T 30,-20");
  Serial.println("CMD,F face_center_x,face_center_y / T angle / M0 angle / M1 angle / H / S / ?");
  Serial.println("UART,USB Serial and Serial2 RX=16 TX=17");
  Serial.println("UNIT,absolute degree relative to power-on home");
  Serial.print("LIMIT,M0,");
  Serial.print(VISION_M0_MIN_DEG, 1);
  Serial.print(",");
  Serial.print(VISION_M0_MAX_DEG, 1);
  Serial.print(",M1,");
  Serial.print(VISION_M1_MIN_DEG, 1);
  Serial.print(",");
  Serial.println(VISION_M1_MAX_DEG, 1);
  Serial.print("DEADBAND,");
  Serial.println(VISION_TARGET_DEADBAND_DEG, 2);
}

void PrintSerialTargetStatus()
{
  Serial.print("TARGET,");
  Serial.print((M0_move_goal_angle - M0_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print((M1_move_goal_angle - M1_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.println(target_move_duration_us / 1000UL);
  Serial.print("LIMIT,M0,");
  Serial.print(VISION_M0_MIN_DEG, 1);
  Serial.print(",");
  Serial.print(VISION_M0_MAX_DEG, 1);
  Serial.print(",M1,");
  Serial.print(VISION_M1_MIN_DEG, 1);
  Serial.print(",");
  Serial.print(VISION_M1_MAX_DEG, 1);
  Serial.print(",DEADBAND,");
  Serial.println(VISION_TARGET_DEADBAND_DEG, 2);
}

void PrintSerialDebug()
{
  if(!SERIAL_DEBUG_OUTPUT)
  {
    return;
  }
  unsigned long now_ms = millis();
  if(now_ms - serial_debug_last_ms < SERIAL_DEBUG_INTERVAL_MS)
  {
    return;
  }
  serial_debug_last_ms = now_ms;

  Serial.print("STAT,");
  Serial.print((M0_target_angle - M0_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print((DFOC_M0_Angle() - M0_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print(M0_target_velocity, 3);
  Serial.print(",");
  Serial.print(DFOC_M0_Velocity(), 3);
  Serial.print(",");
  Serial.print((M1_target_angle - M1_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print((DFOC_M1_Angle() - M1_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print(M1_target_velocity, 3);
  Serial.print(",");
  Serial.println(DFOC_M1_Velocity(), 3);
}

void ReadDriveTestCommand()
{
  static String command;

  while(Serial.available() > 0)
  {
    char ch = (char)Serial.read();
    if(ch == '\r')
    {
      continue;
    }
    if(ch != '\n')
    {
      command += ch;
      if(command.length() > 64)
      {
        command = "";
      }
      continue;
    }

    command.trim();
    if(command.length() == 0)
    {
      return;
    }

    String upper = command;
    upper.toUpperCase();
    if(upper == "?" || upper == "HELP")
    {
      PrintDriveTestHelp();
    }
    else if(upper.startsWith("MODE"))
    {
      int mode = upper.substring(4).toInt();
      if(mode < 0 || mode > 6)
      {
        Serial.println("ERR,mode range is 0..6");
      }
      else
      {
        drive_test_mode = mode;
        drive_test_running = false;
        StopDriveTestOutput();
        PrintDriveTestStatus();
      }
    }
    else if(upper == "RUN")
    {
      drive_test_running = true;
      drive_test_open_angle = 0.0f;
      PrintDriveTestStatus();
    }
    else if(upper == "STOP")
    {
      drive_test_running = false;
      StopDriveTestOutput();
      PrintDriveTestStatus();
    }
    else if(upper == "S")
    {
      PrintDriveTestStatus();
    }
    else if(upper.startsWith("U"))
    {
      drive_test_uq = ClampFloat(command.substring(1).toFloat(), -2.5f, 2.5f);
      PrintDriveTestStatus();
    }
    else if(upper.startsWith("V"))
    {
      drive_test_speed = ClampFloat(command.substring(1).toFloat(), -8.0f, 8.0f);
      PrintDriveTestStatus();
    }
    else if(upper.startsWith("T"))
    {
      String payload = command.substring(1);
      payload.trim();
      if(drive_test_mode == 0 || drive_test_mode == 1)
      {
        drive_test_uq = ClampFloat(payload.toFloat(), -2.5f, 2.5f);
      }
      else if(drive_test_mode == 3)
      {
        drive_test_speed = ClampFloat(payload.toFloat(), -8.0f, 8.0f);
      }
      else
      {
        int comma_pos = payload.indexOf(',');
        if(comma_pos >= 0)
        {
          drive_test_target_deg_m0 = ClampFloat(payload.substring(0, comma_pos).toFloat(), -120.0f, 120.0f);
          drive_test_target_deg_m1 = ClampFloat(payload.substring(comma_pos + 1).toFloat(), -120.0f, 120.0f);
        }
        else
        {
          float target_deg = ClampFloat(payload.toFloat(), -120.0f, 120.0f);
          drive_test_target_deg_m0 = target_deg;
          drive_test_target_deg_m1 = -target_deg;
        }
      }
      PrintDriveTestStatus();
    }
    else
    {
      Serial.println("ERR,unknown command,use ? for help");
    }

    command = "";
  }
}

void RunDriveTestMode()
{
  if(!drive_test_running)
  {
    StopDriveTestOutput();
    return;
  }

  const float m0_target_angle = M0_home_angle + drive_test_target_deg_m0 * PI / 180.0f;
  const float m1_target_angle = M1_home_angle + drive_test_target_deg_m1 * PI / 180.0f;

  switch(drive_test_mode)
  {
    case 0:
      M0_setTorque(drive_test_uq, drive_test_open_angle);
      M1_setTorque(drive_test_uq, -drive_test_open_angle);
      drive_test_open_angle += drive_test_speed * 0.001f;
      if(drive_test_open_angle > 2.0f * PI)
      {
        drive_test_open_angle -= 2.0f * PI;
      }
      break;

    case 1:
      DFOC_M0_setTorque(drive_test_uq);
      DFOC_M1_setTorque(-drive_test_uq);
      break;

    case 2:
    {
      float m0_error_deg = (m0_target_angle - DFOC_M0_Angle()) * 180.0f / PI;
      float m1_error_deg = (m1_target_angle - DFOC_M1_Angle()) * 180.0f / PI;
      DFOC_M0_setTorque(ClampFloat(TARGET_POSITION_KP * m0_error_deg, -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
      DFOC_M1_setTorque(ClampFloat(TARGET_POSITION_KP * m1_error_deg, -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
      break;
    }

    case 3:
      DFOC_M0_setVelocity(drive_test_speed);
      DFOC_M1_setVelocity(-drive_test_speed);
      break;

    case 4:
      DFOC_M0_set_Velocity_Angle(m0_target_angle);
      DFOC_M1_set_Velocity_Angle(m1_target_angle);
      break;

    case 5:
      M0_target_angle = m0_target_angle;
      M1_target_angle = m1_target_angle;
      M0_target_velocity = 0.0f;
      M1_target_velocity = 0.0f;
      MoveTargetCascade();
      break;

    case 6:
      #if ENABLE_IMU_CONTROL
        GetAngle();
        angle1_error_0 = M0_Dir * last_angleY + now;
        Torget_0 = Y_Flt(Y_loop(angle1_error_0 - (DFOC_M0_Angle() * 180.0f / PI)));
        angle1_error_1 = last_angleZ;
        Torget_1 = Z_Flt(Z_loop(angle1_error_1));
        DFOC_M0_setTorque(ClampFloat(Torget_0, -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
        DFOC_M1_setTorque(ClampFloat(M1_Dir * (Torget_1 + 2.9f * sin(last_angleY * PI / 180.0f)), -TARGET_TORQUE_LIMIT, TARGET_TORQUE_LIMIT));
      #else
        StopDriveTestOutput();
        Serial.println("ERR,MODE 6 requires ENABLE_IMU_CONTROL");
        drive_test_running = false;
      #endif
      break;

    default:
      StopDriveTestOutput();
      break;
  }
}

void StopDriveTestOutput()
{
  DFOC_M0_setTorque(0.0f);
  DFOC_M1_setTorque(0.0f);
}

void PrintDriveTestHelp()
{
  Serial.println("DRIVE TEST MENU");
  Serial.println("MODE 0 open-loop electrical angle");
  Serial.println("MODE 1 voltage torque");
  Serial.println("MODE 2 position P");
  Serial.println("MODE 3 velocity loop");
  Serial.println("MODE 4 DengFOC position-velocity cascade");
  Serial.println("MODE 5 custom cascade");
  Serial.println("MODE 6 original gyro gimbal");
  Serial.println("RUN / STOP / S / ?");
  Serial.println("T value: mode0/1=Uq, mode3=rad/s, mode2/4/5=deg or m0,m1");
  Serial.println("U value sets Uq, V value sets rad/s");
}

void PrintDriveTestStatus()
{
  Serial.print("MODE,");
  Serial.print(drive_test_mode);
  Serial.print(",");
  Serial.print(DriveTestModeName(drive_test_mode));
  Serial.print(",");
  Serial.print(drive_test_running ? "RUN" : "STOP");
  Serial.print(",T=");
  Serial.print(drive_test_target_deg_m0, 1);
  Serial.print(",");
  Serial.print(drive_test_target_deg_m1, 1);
  Serial.print(",V=");
  Serial.print(drive_test_speed, 2);
  Serial.print(",U=");
  Serial.println(drive_test_uq, 2);
}

void PrintDriveTestDebug()
{
  unsigned long now_ms = millis();
  if(now_ms - drive_test_debug_last_ms < DRIVE_TEST_DEBUG_INTERVAL_MS)
  {
    return;
  }
  drive_test_debug_last_ms = now_ms;
  Serial.print("STAT,");
  Serial.print(drive_test_mode);
  Serial.print(",");
  Serial.print(drive_test_running ? 1 : 0);
  Serial.print(",");
  Serial.print((DFOC_M0_Angle() - M0_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print(DFOC_M0_Velocity(), 3);
  Serial.print(",");
  Serial.print(DFOC_M0_Current(), 3);
  Serial.print(",");
  Serial.print((DFOC_M1_Angle() - M1_home_angle) * 180.0f / PI, 2);
  Serial.print(",");
  Serial.print(DFOC_M1_Velocity(), 3);
  Serial.print(",");
  Serial.println(DFOC_M1_Current(), 3);
}

const char* DriveTestModeName(int mode)
{
  switch(mode)
  {
    case 0: return "open-loop";
    case 1: return "voltage-torque";
    case 2: return "position-p";
    case 3: return "velocity";
    case 4: return "deng-cascade";
    case 5: return "custom-cascade";
    case 6: return "gyro-gimbal";
    default: return "unknown";
  }
}

float ClampFloat(float value, float min_value, float max_value)
{
  if(value < min_value)
  {
    return min_value;
  }
  if(value > max_value)
  {
    return max_value;
  }
  return value;
}

float SmoothStep5(float progress)
{
  return progress * progress * progress * (progress * (progress * 6.0f - 15.0f) + 10.0f);
}

float SmoothStep5Derivative(float progress)
{
  return 30.0f * progress * progress * (progress - 1.0f) * (progress - 1.0f);
}

/************************ 电源电压检测 ************************/
float GetVinVolt() {
  float Volts = 0;
  if(DengFOC)
  {
    Volts = analogReadMilliVolts(13) * 8.5 / 1000;
  }
  else
  {
    Volts = analogReadMilliVolts(4) * 11 / 1000;
  }

  return Volts;
}

void CheckVinVolt()
{
  float vinVolt = GetVinVolt();
  while (vinVolt <= UNDERVOLTAGE_THRES) {
    vinVolt = GetVinVolt();
    delay(100);
    Serial.printf("等待上电,当前电压%.2f\n", vinVolt);
  }
  Serial.printf("正在校准电机...当前电压%.2f\n", vinVolt);
}

/************************ 写入EEPROM ************************/
void WriteFloatToEEPROM(int addr, float value) {

  //边界检查
  if (addr < 0 || addr + 3 >= EEPROM.length()) {
    Serial.print("写入失败：地址越界！addr=");
    Serial.println(addr);
    return; // 越界则直接退出，避免错误写入
  }

  union {
    float f;
    uint8_t bytes[4];
  } floatData;
  floatData.f = value;
  for (int i = 0; i < 4; i++) {
    EEPROM.write(addr + i, floatData.bytes[i]);
  }

  if (EEPROM.commit()) {
    Serial.println("数据写入EEPROM成功！");
  } else {
    Serial.println("数据写入EEPROM失败！");
  }

}

void WriteIntToEEPROM(int addr, int value) {

  //边界检查
  if (addr < 0 || addr + 3 >= EEPROM.length()) {
    Serial.print("写入失败：地址越界！addr=");
    Serial.println(addr);
    return; // 越界则直接退出，避免错误写入
  }

  union {
    int f;
    uint8_t bytes[4];
  } floatData;
  floatData.f = value;
  for (int i = 0; i < 4; i++) {
    EEPROM.write(addr + i, floatData.bytes[i]);
  }

  if (EEPROM.commit()) {
    Serial.println("数据写入EEPROM成功！");
  } else {
    Serial.println("数据写入EEPROM失败！");
  }

}

/************************ 读取EEPROM ************************/
float ReadFloatToEEPROM(int addr) {

  // 边界检查：防止地址越界（假设 EEPROM 总大小为 512 字节）
  if (addr < 0 || addr + 3 >= EEPROM.length()) {
    Serial.println("EEPROM 地址越界！");
    return -1; // 返回数字，以防后续错误
  }

  union {
    float f;
    uint8_t bytes[4];
  } floatData;
  for (int i = 0; i < 4; i++) {
    floatData.bytes[i]=EEPROM.read(addr + i);
  }
  return floatData.f;
}

int ReadIntToEEPROM(int addr) {

  // 边界检查：防止地址越界（假设 EEPROM 总大小为 512 字节）
  if (addr < 0 || addr + 3 >= EEPROM.length()) {
    Serial.println("EEPROM 地址越界！");
    return -1; // 返回数字，以防后续错误
  }

  union {
    int f;
    uint8_t bytes[4];
  } floatData;
  for (int i = 0; i < 4; i++) {
    floatData.bytes[i]=EEPROM.read(addr + i);
  }
  return floatData.f;
}

/************************ 初始化陀螺仪 ************************/
void Bmi160Init()
{
  BMI160.begin(BMI160GenClass::I2C_MODE,Wire,0X69);//使能BMI，地址为0X69，默认wrie
  BMI160.autoCalibrateGyroOffset();//自动硬件校准         
  BMI160.setGyroRange(1000);  //设置量程为1000
  BMI160.setGyroRate(100);    //陀螺仪采样频率

}

/************************ 陀螺仪手动校准 ************************/
void CalibrateGyro() {
  Serial.println("开始校准陀螺仪");
  float gx_sum = 0, gy_sum = 0, gz_sum = 0;
  
  // 多次采样求平均
  for (int i = 0; i < 1000; i++) {
    BMI160.readGyro(gx, gy, gz);

    //修正方向
    cx = gy;
    cy = -gx;
    cz = gz;

    // //修正方向
    // cx = gx;
    // cy = gy;
    // cz = gz;


    // 换算为dps（1000量程下，1 LSB = 1000/32768 ≈ 0.0305 dps）
    float gx_dps = (float)cx / 32.8;
    float gy_dps = (float)cy / 32.8;
    float gz_dps = (float)cz / 32.8;
    
    gx_sum += gx_dps;
    gy_sum += gy_dps;
    gz_sum += gz_dps;
    
    delay(2); // 采样间隔
  }
  
  // 计算偏移值（静止时的零漂）
  gyroXoffset = gx_sum / 1000;
  gyroYoffset = gy_sum / 1000;
  gyroZoffset = gz_sum / 1000;

  Serial.println("校准陀螺仪完成");
}

/************************ 获取角度 ************************/
void GetAngle() 
{
  BMI160.readAccelerometer(ax, ay, az);//读取加速度
  BMI160.readGyro(gx,gy,gz);        //读取角速度

  //陀螺仪方向匹配
  // bx = az;
  // by = -ax;
  // bz = -ay;
  // cx = gz;
  // cy = -gx;
  // cz = -gy;

  // bx = ax;
  // by = ay;
  // bz = az;
  // cx = gx;
  // cy = gy;
  // cz = gz;

  bx = ay;
  by = -ax;
  bz = az;
  cx = gy;
  cy = -gx;
  cz = gz;

  accX = ((float)bx) / 16384.0;//换算加速度
  accY = ((float)by) / 16384.0;
  accZ = ((float)bz) / 16384.0;

  angleAccX = atan2(accY, sqrt(accX*accX + accZ*accZ)) * 180.0 / PI;
  angleAccY = atan2(-accX, sqrt(accY*accY + accZ*accZ)) * 180.0 / PI;

  gyroX = ((float)cx) / 32.8 - gyroXoffset;//量程1000
  gyroY = ((float)cy) / 32.8 - gyroYoffset;//量程1000
  gyroZ = ((float)cz) / 32.8 - gyroZoffset;//量程1000

  // 死区处理：过滤小于0.1dps的噪声，避免积分累积
  gyroX = fabs(gyroX) < DEAD_ZONE ? 0 : gyroX;
  gyroY = fabs(gyroY) < DEAD_ZONE ? 0 : gyroY;
  // gyroZ = fabs(gyroZ) < DEAD_ZONE ? 0 : gyroZ;
   gyroZ = fabs(gyroZ) < 0.1 ? 0 : gyroZ;

  //低通滤波，角度积分
  filteredGyroX = 0.9 * filteredGyroX + (1.0f - 0.9) * gyroX;
  filteredGyroY = 0.9 * filteredGyroY + (1.0f - 0.9) * gyroY;
  filteredGyroZ = 0.98 * filteredGyroZ + (1.0f - 0.98) * gyroZ;

  // filteredGyroZ = gyroZ;

  interval = (millis() - preInterval) * 0.001;//获取系统时间

  last_angleX = (0.99f * (last_angleX + filteredGyroX * interval)) + (0.01f * angleAccX);//互补滤波
  last_angleY = (0.99f * (last_angleY + filteredGyroY * interval)) + (0.01f * angleAccY);
  if (fabs(filteredGyroZ) > 0.1f) {
  last_angleZ = last_angleZ + filteredGyroZ * interval ;
  }
  preInterval = millis();
}
