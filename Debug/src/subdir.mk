################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CC_SRCS += \
../src/main.cc 

CPP_SRCS += \
../src/HTTPClient.cpp \
../src/HardwareManager.cpp \
../src/LoggerController.cpp \
../src/MQTTController.cpp \
../src/SerialManager.cpp \
../src/ThreadSafeQueue.cpp 

CC_DEPS += \
./src/main.d 

OBJS += \
./src/HTTPClient.o \
./src/HardwareManager.o \
./src/LoggerController.o \
./src/MQTTController.o \
./src/SerialManager.o \
./src/ThreadSafeQueue.o \
./src/main.o 

CPP_DEPS += \
./src/HTTPClient.d \
./src/HardwareManager.d \
./src/LoggerController.d \
./src/MQTTController.d \
./src/SerialManager.d \
./src/ThreadSafeQueue.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 Linux g++ compiler'
	arm-linux-gnueabihf-g++ -Wall -O0 -g3 -I../lib/inc/ -I../lib/inc/paho -c -fmessage-length=0 -MT"$@" -mno-unaligned-access -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/%.o: ../src/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 Linux g++ compiler'
	arm-linux-gnueabihf-g++ -Wall -O0 -g3 -I../lib/inc/ -I../lib/inc/paho -c -fmessage-length=0 -MT"$@" -mno-unaligned-access -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


