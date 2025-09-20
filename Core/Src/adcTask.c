/* Includes */
#include "adcTask.h"
#include "cmsis_os.h"
#include "adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include "smf.h"
#include <limits.h>

/* Defines */

#define VREFINT_CAL_ADDR ((uint16_t*) ((uint32_t)0x1FFFF7BAU))
#define adc_value 0x01
#define uart_value 0x02

/* Global Variables */
float temp;
float voltaj;
float Vref;
uint32_t adc_Vref;
uint32_t raw;
static uint16_t Vref_cal= 0;
uint16_t buffer[2];
char text[32];
uint8_t TxData[32];
uint32_t value= 0;

/* Variables */
extern osThreadId_t Task2Handle;
extern osThreadId_t Task3Handle;
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;
static const struct smf_state states[];

enum adc_states {
    state_waiting,
    state_calculating,
    state_uart
};

void waiting_entry(struct smf_ctx *ctx) {
	Vref_cal = *VREFINT_CAL_ADDR;
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)buffer, 2);
}
void waiting_run(struct smf_ctx *ctx) {

	xTaskNotifyWait(0x00, ULONG_MAX, &value, portMAX_DELAY);
	if(value & adc_value){
		adc_Vref= buffer[0];
	    raw= buffer[1];
		smf_set_state(ctx, &states[state_calculating]);
	}
}
void calculating_entry(struct smf_ctx *ctx) {
	Vref = 3.3f *((float)Vref_cal / (float)adc_Vref);
	voltaj= ((float)raw* Vref)/4096.0f;
	temp = ((1.43f - voltaj)  /0.0043f) + 25.0f;
	HAL_ADC_Stop_DMA(&hadc1);
}
void calculating_run(struct smf_ctx *ctx) {
	smf_set_state(ctx, &states[state_uart]);
}

void uart_entry(struct smf_ctx *ctx) {
	snprintf(text, sizeof(text), "temp: %.2f\n", temp);
	HAL_UART_Transmit_DMA(&huart1,  (uint8_t*)text, strlen(text));
}
void uart_run(struct smf_ctx *ctx) {
	// Wait for tranmission complete ulTaskNotifyTake
	// on sucess go to state waiting smf_set_state(&adcCtx, &states[state_waiting]);
	xTaskNotifyWait(0x00, ULONG_MAX, &value, portMAX_DELAY);
	if(value & uart_value){
	printf(text);
	smf_set_state(ctx, &states[state_waiting]);
	}
}

static const struct smf_state states[] = {
	[state_waiting] = SMF_CREATE_STATE(waiting_entry, waiting_run, NULL, NULL, NULL),
	[state_calculating] = SMF_CREATE_STATE(calculating_entry, calculating_run, NULL, NULL, NULL),
	[state_uart] = SMF_CREATE_STATE(uart_entry, uart_run, NULL, NULL, NULL)
};
struct smf_ctx adcCtx;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(Task2Handle, adc_value , eSetBits, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(Task2Handle, uart_value  , eSetBits, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    // Notify the task
}

void TaskFunction2(void *argument) {

    smf_set_initial(&adcCtx, &states[state_waiting]);
	for(;;) {
		smf_run_state(&adcCtx);
	}
}
	// TOOD: Task Func2 here
