#include <Arduino.h>

#include "DollhousePanel.h"

#include <Adafruit_TLC59711.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <Fsm.h>
#include <EnableInterrupt.h>

const char NUM_TLC59711=1;
const char TLC_DATA=12;
const char TLC_CLK=13;

const char PIXEL_COUNT = 3;
const char PIXEL_PIN = 8;

enum events {
  CHANGE_LIGHT_MODE,
  NEXT_ROOM,
  PREVIOUS_ROOM,
  RESET_ROOMS
};

// Lighting modes finite state machine
State state_lighting_mode(on_lighting_mode_enter, &on_lighting_mode_exit);
State state_party_mode(on_party_mode_enter, &on_party_mode_exit);
State state_nitelite_mode(on_nitelite_mode_enter, &on_nitelite_mode_exit);
State state_off_mode(on_off_mode_enter, &on_off_mode_exit);
Fsm modes(&state_off_mode);

// Rooms finite state machine
State state_all_rooms(on_all_enter, &on_all_exit);
State state_hall(on_hall_enter, &on_hall_exit);
State state_living_room(on_living_room_enter, &on_living_room_exit);
State state_kitchen(on_kitchen_enter, &on_kitchen_exit);
State state_bedroom(on_bedroom_enter, &on_bedroom_exit);
State state_bathroom(on_bathroom_enter, &on_bathroom_exit);
State state_attic(on_attic_enter, &on_attic_exit);
Fsm rooms(&state_all_rooms);

// LastROOM is included to make it easier to figure out the size of the enum
// for things like sizing the brightness state array
enum Rooms {
  ALL_ROOMS,
  LIVING_ROOM,
  HALL,
  KITCHEN,
  BEDROOM,
  BATHROOM,
  ATTIC,
  LastROOM
};

// NeoPixels (for the attic & !!!PARTY MODE!!!)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// PWM board (controls the room lights)
Adafruit_TLC59711 tlc = Adafruit_TLC59711(NUM_TLC59711, TLC_CLK, TLC_DATA);

// 16x2 LCD display
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

// Panel RGB LED pins
const char RED_PIN = 9;
const char GREEN_PIN = 10;
const char BLUE_PIN = 11;

int brightness = 90;
int roomBrightness[LastROOM];
int currentRoom = ALL_ROOMS;

void setup() {
  // Fire up the LCD display
  lcd.begin(16, 2);
  lcd.print("Doll house");
  lcd.setCursor(0,1);
  lcd.print("lighting!");

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // initialize the NeoPixel strand
  strip.begin();
  strip.show();

  // Initialize the PWM board
  tlc.begin();
  tlc.write();

  // set defualt room brightness
  for (int i = 0; i != LastROOM; i++) {
    roomBrightness[i] = {brightness * 180};
  }

  // enable interrupts on buttons
  // The button interface is a Smartmaker 5A5 (annoying, but it works)
  enableInterrupt(A0, handleButtonOne, FALLING);
  enableInterrupt(A1, handleButtonTwo, FALLING);
  enableInterrupt(A2, handleButtonThree, FALLING);
  enableInterrupt(A3, handleButtonFour, FALLING);
  enableInterrupt(A4, handleButtonFive, FALLING);

  // mode FSM transitions
  modes.add_transition(&state_off_mode, &state_lighting_mode, CHANGE_LIGHT_MODE, NULL);
  modes.add_transition(&state_lighting_mode, &state_party_mode, CHANGE_LIGHT_MODE, NULL);
  modes.add_transition(&state_party_mode, &state_nitelite_mode, CHANGE_LIGHT_MODE, NULL);
  modes.add_transition(&state_nitelite_mode, &state_off_mode, CHANGE_LIGHT_MODE, NULL);

  // rooms FSM transitions
  // looping "forward" through the rooms
  rooms.add_transition(&state_all_rooms, &state_hall, NEXT_ROOM, NULL);
  rooms.add_transition(&state_hall, &state_living_room, NEXT_ROOM, NULL);
  rooms.add_transition(&state_living_room, &state_kitchen, NEXT_ROOM, NULL);
  rooms.add_transition(&state_kitchen, &state_bedroom, NEXT_ROOM, NULL);
  rooms.add_transition(&state_bedroom, &state_bathroom, NEXT_ROOM, NULL);
  rooms.add_transition(&state_bathroom, &state_attic, NEXT_ROOM, NULL);
  rooms.add_transition(&state_attic, &state_all_rooms, NEXT_ROOM, NULL);

  // looping "backward" through the rooms
  rooms.add_transition(&state_all_rooms, &state_attic, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_attic, &state_bathroom, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_bathroom, &state_bedroom, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_bedroom, &state_kitchen, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_kitchen, &state_living_room, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_living_room, &state_hall, PREVIOUS_ROOM, NULL);
  rooms.add_transition(&state_hall, &state_all_rooms, PREVIOUS_ROOM, NULL);

  // reseting to the default room (all rooms)
  rooms.add_transition(&state_hall, &state_all_rooms, RESET_ROOMS, NULL);
  rooms.add_transition(&state_living_room, &state_all_rooms, RESET_ROOMS, NULL);
  rooms.add_transition(&state_kitchen, &state_all_rooms, RESET_ROOMS, NULL);
  rooms.add_transition(&state_bedroom, &state_all_rooms, RESET_ROOMS, NULL);
  rooms.add_transition(&state_bathroom, &state_all_rooms, RESET_ROOMS, NULL);
  rooms.add_transition(&state_attic, &state_all_rooms, RESET_ROOMS, NULL);
}

void handleButtonOne() {
  lcd.clear();
  rooms.trigger(RESET_ROOMS);
  modes.trigger(CHANGE_LIGHT_MODE);
}

void handleButtonTwo() {
  lcd.clear();
  lcd.print("Brightness++");
  lcd.setCursor(0,1);
  brightness = min(brightness + 30, 180);
  lcd.print(brightness);
  setRGBColor(0,0,brightness);
  setRoomBrightness(currentRoom,brightness * 180);
  delay(200);
}

void handleButtonThree() {
  lcd.clear();
  rooms.trigger(PREVIOUS_ROOM);
}

void handleButtonFour() {
  lcd.clear();
  lcd.print("Brightness--");
  lcd.setCursor(0,1);
  brightness = max(brightness - 30, 0);
  lcd.print(brightness);
  setRGBColor(0,0,brightness);
  setRoomBrightness(currentRoom,brightness * 180);
  delay(200);
}

void handleButtonFive() {
  lcd.clear();
  rooms.trigger(NEXT_ROOM);
}

void setRGBColor(int red, int green, int blue) {
  int myRed = constrain(red, 0, 255);
  int myGreen = constrain(green, 0, 255);
  int myBlue = constrain(blue, 0, 255);

  analogWrite(RED_PIN, myRed);
  analogWrite(GREEN_PIN, myGreen);
  analogWrite(BLUE_PIN, myBlue);
}

void setRoomBrightness(int room, int level) {
  roomBrightness[room] = level;
  tlc.setPWM(room * 3, roomBrightness[room]);
  tlc.write();
}

void lightRooms() {
  for (int i = 0; i < 6; i++) {

  }
}

// ***** FSM event handlers ***** //

// ---- lighting mode states ---- //

void on_lighting_mode_enter(){
  lcd.clear();
  lcd.print("lighting mode!");
}

void on_lighting_mode_exit(){
  
}

void on_party_mode_enter(){
  lcd.clear();
  lcd.print("party mode!");
}

void on_party_mode_exit(){
  
}

void on_nitelite_mode_enter(){
  lcd.clear();
  lcd.print("nitelite mode!");  
}

void on_nitelite_mode_exit(){

}

void on_off_mode_enter(){
  lcd.clear();
  lcd.print("off mode!");  
}

void on_off_mode_exit(){

}

// ---- room selection states ---- //
void on_all_enter() {
  lcd.clear();
  currentRoom = ALL_ROOMS;
  lcd.print("all, B:");
  lcd.print(roomBrightness[currentRoom]);
}

void on_all_exit() {
  
}

void on_hall_enter() {
  lcd.clear();
  currentRoom = HALL;
  lcd.print("Setting hall");
}

void on_hall_exit() {
  
}

void on_living_room_enter() {
  lcd.clear();
  currentRoom = LIVING_ROOM;
  lcd.print("Setting living room");
}

void on_living_room_exit() {
  
}

void on_kitchen_enter() {
  lcd.clear();
  currentRoom = KITCHEN;
  lcd.print("Setting kitchen");
}

void on_kitchen_exit() {
  
}

void on_bathroom_enter() {
  lcd.clear();
  currentRoom = BATHROOM;
  lcd.print("Setting bathroom");
}

void on_bathroom_exit() {
  
}

void on_bedroom_enter() {
  lcd.clear();
  currentRoom = BEDROOM;
  lcd.print("Setting bedroom");
}

void on_bedroom_exit() {
  
}

void on_attic_enter() {
  lcd.clear();
  currentRoom = ATTIC;
  lcd.print("Setting attic");
}

void on_attic_exit() {
  
}

void loop() {
  setRGBColor(0,0,brightness);
}
