// We'll name the Sabertooth object ST.
// For how to configure the Sabertooth, see the DIP Switch Wizard for
// https://www.dimensionengineering.com/datasheets/Sabertooth2x60.pdf
// Be sure to select Simplified Serial Mode for use with this library.
// This sample uses a baud rate of 9600 (page 6 and 16 on Sabertotth2x60 manual).
//
// Connections to make (See Gunther_Assembly_Manual):
//   Arduino TX->1  ->  Sabertooth S1
//   Arduino GND    ->  Sabertooth 0V
//   Arduino VIN    ->  Sabertooth 5V (OPTIONAL, if you want the Sabertooth to power the Arduino)
//   Arduino PIN 3  ->  Encoder white cable LA 
//   Arduino PIN 4  ->  Encoder white cable LB
//   Arduino PIN 5  ->  Encoder white cable RA
//   Arduino PIN 6  ->  Encoder white cable RB
//   https://learn.parallax.com/tutorials/robot/arlo/arlo-robot-assembly-guide/section-1-motor-mount-and-wheel-kit-assembly/step-6
//
// If you want to use a pin other than TX->1, see the SoftwareSerial example.


#define USBCON // RX and TX Arduino Leonardo - Sabertooth
#define USE_USBCON // ROS Arduino Leonardo
#define L 0.5 // distance between axes
#define R 0.0775 // wheel radius 

#include <ArduinoHardware.h>
#include <ros.h>
#include <ros/time.h>
#include <math.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <geometry_msgs/Point32.h>
#include <SabertoothSimplified.h>

SabertoothSimplified ST;

geometry_msgs::Twist vel_ref;
geometry_msgs::Vector3Stamped vel_encoder_robo;
geometry_msgs::Point32 vel_kinematic_robo;

char encoder[] = "/encoder";

//Left wheel
int encoder0PinA_Left = 3;
//int encoder0PinB_Left = 4;
int encoderPinALast_Left = LOW;
int encoder0Pos_Left = 0;
float vel_Left = 0;

//Right wheel
int encoder0PinA_Right = 4;
//int encoder0PinB_Right = 10;
int encoderPinALast_Right= LOW;
int encoder0Pos_Right = 1;
float vel_Right = 0;

//Position encoder Left
int read_Left = LOW;
//float Delta_t_Left = 0;
float Sum_t_Left = 0;
float Sum_vel_Left = 0;
float PreviusMillis_Left = 0;
int cont_Left = 1;

//Position encoder Right
int read_Right = LOW;
float Delta_t_Right = 0;
float Sum_t_Right = 0;
float Sum_vel_Right = 0;
float PreviusMillis_Right = 0;
int cont_Right = 0;

//Controller variable
float epx_Right = 0;
float epx_Left = 0;

int count_iterations = 100;
int i = 0;

double PreviusMillis = 0;

//ROS Function - Angular and linear Velocity Desired
void velRasp(const geometry_msgs::Twist& msg){
  
  //V - linear velocity disired
  vel_ref.linear.x = msg.linear.x;

  //W - angular velocity disired
  vel_ref.angular.z = msg.angular.z;
}

// Robot Differential Drive Kinematic
void kinematic(){

  vel_kinematic_robo.x = ( (2*vel_ref.linear.x) - (vel_ref.angular.z*L) )/2;  // Left wheel
  vel_kinematic_robo.y = ( (2*vel_ref.linear.x) + (vel_ref.angular.z*L) )/2;  // Right wheel
}

ros::NodeHandle  nh;
//Subscribers
ros::Subscriber<geometry_msgs::Twist> sub_rasp("/cmd_vel", &velRasp);
//Publisher
ros::Publisher pub_encoder("/vel_encoder", &vel_encoder_robo);

//int count_left=0, count_right=0;
void getEncoderCount(){
  if ((encoderPinALast_Left == LOW) && (read_Left == HIGH)) {
    encoder0Pos_Left++;
  }

  if ((encoderPinALast_Right == LOW) && (read_Right == HIGH)) {
    encoder0Pos_Right++;
  }
}

// *********************************************
//Left wheel control
void RosController_Wheel_Left() {

  //Call reference speed from kinematic
  float w_left = vel_kinematic_robo.x;// + (vel_kinematic_robo.x*0.0672);

  // Debug kinematic
  vel_encoder_robo.vector.z = w_left;

  // Tangential velocity measured by encoder sensor - Vel_Left
  double Delta_t_Left = (millis() - PreviusMillis_Left) * 0.001;
  PreviusMillis_Left = millis();

  //Linear speed with respect to Theta = 10 degrees (encoder sensitivity) of wheel displacement of R = 7.5 cm radius.
  double w = (10 * PI / 180) / (Delta_t_Left);
  vel_Left = encoder0Pos_Left*w*R; // Count of pin_Left encoder0Pos_Left;

  Sum_vel_Left = Sum_vel_Left + vel_Left;

  //Average speed of a wheel V_linear
  float Media_Vl_encoder = Sum_vel_Left / cont_Left;

  //Mean of velocity in 20 interations
  cont_Left++;
  if(cont_Left>20){
    Sum_vel_Left = vel_Left;
    cont_Left = 1;
  }

  //DEBUG
  vel_encoder_robo.vector.x = Media_Vl_encoder;
  vel_encoder_robo.vector.y = vel_Left;


  float Vl_gain;

  //V_linear controll erro = (cinematica - encoder)
  float erro = abs(w_left) - Media_Vl_encoder;
  //Proportional gain
  float kp = 0.7;
  //Integrative Gain
  float ki = 0.007;

  //if (abs(erro) > 0.001) {
    //PID control
    float u = (erro * kp) + ((erro + epx_Left) * ki);
    //Integrator Cumulative Error
    epx_Left = epx_Left + erro;

    //Change the sinal of controll
    if(w_left == 0){
      //Reset commands
      Media_Vl_encoder = 0;
      encoder0Pos_Left = 0;
      Sum_vel_Left = 0;
      epx_Left = 0;
      Vl_gain = 0;
      //Publisher Encoder Debug
      vel_encoder_robo.vector.x = 0;

    }else if(w_left < 0){
      u = u*(-1);
      //Speed saturation conversion
      Vl_gain = round((127 * u)/0.6);

      //Publisher Encoder Debug
      //vel_encoder_robo.vector.x = vel_Left*(-1);
    }else{
      //Speed saturation conversion
      Vl_gain = round((127 * u)/0.6);
      //Publisher Encoder Debug
      //vel_encoder_robo.header.stamp = nh.now();

      // //Limitation of controller
      // if(abs(Vl_gain > 127)){
      //   Vl_gain = (u/abs(u))*127;
      // }

      //vel_encoder_robo.vector.y = vel_Left;
    }
    //vel_encoder_robo.header.stamp = nh.now();

    //Output Motor Left
    ST.motor(2, Vl_gain);// vl
  //}
}

//Left wheel control
void RosController_Wheel_Right() {

  //Call reference speed from kinematic
  float w_right = vel_kinematic_robo.y;

  //Debug kinematic
  vel_encoder_robo.vector.z = w_right;// - (w_right*0.004);

  //Tangential velocity measured by encoder sensor - Vel_Left
  read_Right = digitalRead(encoder0PinA_Right);
  
  //Debug
  //vel_encoder_robo.vector.x = read_Right;

  if ((encoderPinALast_Right == LOW) && (read_Right == HIGH)) {
    // if (digitalRead(encoder0PinB_Left) == HIGH) {
      encoder0Pos_Right++;
      // Time between encoder signals
      Delta_t_Right = (millis() - PreviusMillis_Right) * 0.001;
      PreviusMillis_Right = millis();

      //Linear speed with respect to Theta = 10 degrees (encoder sensitivity) of wheel displacement of R = 7.5 cm radius.
      float w = (10 * PI / 180) / (Delta_t_Right);
      vel_Right = w*R;
      Sum_vel_Right = Sum_vel_Right + vel_Right;

      //Mean of velocity in 30 interations
      cont_Right++;
      if(cont_Right>10){
        Sum_vel_Right = vel_Right;
        encoder0Pos_Right = 1;
        cont_Right = 0;
      }
    //}
  }

  encoderPinALast_Right = read_Right;
  //Average speed of a wheel V_linear
  float Media_Vr_encoder = Sum_vel_Right / encoder0Pos_Right;

  float Vl_gain;

  //V_linear controll erro = (cinematica - encoder)
  float erro = abs(w_right) - Media_Vr_encoder;
  //Proportional gain
  float kp = 0.7;
  //Integrative Gain
  float ki = 0.007;

  if (abs(erro) > 0.001) {
    //PID control
    float u = (erro * kp) + ((erro + epx_Right) * ki);
    //Integrator Cumulative Error
    epx_Right = epx_Right + erro;

    //Change the sinal of controll
    if(w_right == 0){
      //Reset commands
      Media_Vr_encoder = 0;
      encoder0Pos_Right = 1;
      Sum_vel_Right = 0;
      epx_Right = 0;
      Vl_gain = 0;
      //Publisher Encoder Debug
      vel_encoder_robo.vector.y = 0;

    }else if(w_right < 0){
      u = u*(-1);
      //Speed saturation conversion
      Vl_gain = round((127 * u)/0.6);

      //Limitation of controller
      // if(abs(Vl_gain > 127)){
      //   Vl_gain = (u/abs(u))*127;
      // }

      //Publisher Encoder Debug
      vel_encoder_robo.vector.y = vel_Right*(-1);
    }else{
      //Speed saturation conversion
      Vl_gain = round((127 * u)/0.6);

      //Limitation of controller
      // if(abs(Vl_gain > 127)){
      //   Vl_gain = (u/abs(u))*127;
      // }

      //Publisher Encoder Debug
      vel_encoder_robo.vector.y = vel_Right;
    }

    //Degug-ROS
    //vel_encoder_robo.vector.x = Vl_gain;

    //Output Motor Left
    ST.motor(1, -Vl_gain);// vl
  }
}

// *********************************************

void setup()
{
  SabertoothTXPinSerial.begin(9600); // This is the baud rate you chose with the DIP switches.
  //Serial.begin(9600);
  delay(5000);
  pinMode (encoder0PinA_Left, INPUT);
  pinMode (encoder0PinA_Right, INPUT);
// ROS Initialization with Publishers and Subscribers 
  nh.initNode();
  nh.subscribe(sub_rasp);
  nh.advertise(pub_encoder);

}



void loop()
{
  nh.spinOnce();
  read_Left = digitalRead(encoder0PinA_Left);
  //read_Right = digitalRead(encoder0PinA_Right);
  getEncoderCount();

  if((count_iterations) == 0){
    kinematic();                                     
    RosController_Wheel_Left();
    //RosController_Wheel_Right();
    vel_encoder_robo.header.stamp = nh.now();
    vel_encoder_robo.header.frame_id = encoder;
    pub_encoder.publish(&vel_encoder_robo);
    count_iterations = 100;
    encoder0Pos_Left = 1;
  }
  count_iterations--;
  encoderPinALast_Left = read_Left;
  delay(1);
}