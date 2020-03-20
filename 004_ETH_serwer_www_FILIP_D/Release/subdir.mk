################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../enc28j60.c \
../fun.c \
../ip_arp_udp_tcp.c \
../main.c \
../web_page.c \
../websrv_help_functions.c 

OBJS += \
./enc28j60.o \
./fun.o \
./ip_arp_udp_tcp.o \
./main.o \
./web_page.o \
./websrv_help_functions.o 

C_DEPS += \
./enc28j60.d \
./fun.d \
./ip_arp_udp_tcp.d \
./main.d \
./web_page.d \
./websrv_help_functions.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: AVR Compiler'
	avr-gcc -Wall -Os -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -std=gnu99 -funsigned-char -funsigned-bitfields -mmcu=atmega328p -DF_CPU=12500000UL -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


