################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../basic_web_server_example.c \
../enc28j60.c \
../fun.c \
../ip_arp_udp_tcp.c \
../websrv_help_functions.c 

OBJS += \
./basic_web_server_example.o \
./enc28j60.o \
./fun.o \
./ip_arp_udp_tcp.o \
./websrv_help_functions.o 

C_DEPS += \
./basic_web_server_example.d \
./enc28j60.d \
./fun.d \
./ip_arp_udp_tcp.d \
./websrv_help_functions.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: AVR Compiler'
	avr-gcc -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=atmega328p -DF_CPU=12500000UL -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


