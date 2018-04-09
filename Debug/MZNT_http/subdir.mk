################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../MZNT_http/http-client.cpp 

OBJS += \
./MZNT_http/http-client.o 

CPP_DEPS += \
./MZNT_http/http-client.d 


# Each subdirectory must supply rules for building sources it contributes
MZNT_http/%.o: ../MZNT_http/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 Linux g++ compiler'
	arm-linux-gnueabihf-g++ -Wall -O0 -g3 -I/home/chris/workspace/paho-port/src -I/home/chris/workspace/curl-port/build/include/ -c -fmessage-length=0 -MT"$@" -mno-unaligned-access -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


