/*******************************************************************************
* Copyright (C) 2021 Maxim Integrated Products, Inc., All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

#include <MAX31889/MAX31889.h>


#define GET_BIT_VAL(val, pos, mask)     ( ( (val) & mask) >> pos )
#define SET_BIT_VAL(val, pos, mask)     ( ( ((int)val) << pos) & mask )

//
#define GPIO_MODE_MASK(idx)               (3<<(idx*6))
#define GPIO_MODE_SET_MODE(idx, mode)     (mode<<(idx*6))
#define GPIO_MODE_GET_MODE(idx, val)      ( (val>>(idx*6)) & 0x03)
#define GPIO_LOGIC_LEVEL_MASK(idx)        (1<<(idx*3))
//
#define TEMP_PER_BIT                      (0.005f) // 16bit resolution

/************************************ Private Functions ***********************/
int MAX31889::read_register(uint8_t reg, uint8_t *data, int len)
{
    int ret;
    int counter = 0;

    m_i2c->beginTransmission(m_slave_addr);
    m_i2c->write(reg);

    /*
        stop = true, sends a stop message after transmission, releasing the I2C bus.
        
        stop = false, sends a restart message after transmission. 
          The bus will not be released, which prevents another master device from transmitting between messages. 
          This allows one master device to send multiple transmissions while in control.
    */
    ret = m_i2c->endTransmission(false);
    /*
        0:success
        1:data too long to fit in transmit buffer
        2:received NACK on transmit of address
        3:received NACK on transmit of data
        4:other error
    */  
    if (ret != 0) {
        m_i2c->begin(); // restart
        return -1;
    }

    // READ
    m_i2c->requestFrom((char)m_slave_addr, (char)len, false);

    while (m_i2c->available()) { // slave may send less than requested
        data[counter++] = m_i2c->read(); // receive a byte as character
    }
        
    ret = (counter == len) ? 0: -1;

    m_i2c->endTransmission();

    if (ret != 0) {
        m_i2c->begin(); // restart
    }

    return ret;
}

int MAX31889::write_register(uint8_t reg, const uint8_t *data, int len)
{
    int ret;

    m_i2c->beginTransmission(m_slave_addr);
    //
    m_i2c->write(reg);
    m_i2c->write(data, len);
    //
    ret = m_i2c->endTransmission();
    /*
        0:success
        1:data too long to fit in transmit buffer
        2:received NACK on transmit of address
        3:received NACK on transmit of data
        4:other error
    */

    if (ret != 0) {
        m_i2c->begin(); // restart
    }

    return ret;
}

uint16_t MAX31889::convert_temp_2_count(float temp)
{
    uint16_t count;

    if (temp < 0) {
        temp  = 0 - temp;
        count = temp / TEMP_PER_BIT;
        count = (count - 1) ^ 0xFFFF;
    } else {
        count = temp/TEMP_PER_BIT;
    }

    return count;
}

float MAX31889::convert_count_2_temp(uint16_t count)
{
    float temp;

    // 16th bit is sign bit
    if (count & (1<<15) ) {
        // it is 2 complement
        count = (count ^ 0xFFFF) + 1;
        temp  = count * TEMP_PER_BIT;
        temp  = 0 - temp; // convert to negative
    } else {
        temp = count * TEMP_PER_BIT;
    }

    return temp;
}

/************************************ Public Functions ***********************/
MAX31889::MAX31889(TwoWire *i2c, uint8_t i2c_addr)
{
    if (i2c == NULL) {
        while (1) {
            ;
        }
    }
    
    m_slave_addr = i2c_addr;
    m_i2c = i2c;
}

void MAX31889::begin(void)
{
    m_i2c->begin();
    reset_registers();
}

int MAX31889::reset_registers(void)
{
    int ret = 0;
    uint8_t val8 = 1;

    ret = write_register(MAX31889_R_SYS_CTRL, &val8);

    return ret;
}

int MAX31889::get_status(reg_status_t &stat)
{
    int ret = 0;
    uint8_t val8;

    ret = read_register(MAX31889_R_STATUS, &val8);
    if (ret) {
        return ret;
    }

    stat.bits.a_full    = GET_BIT_VAL(val8, MAX31889_F_STATUS_A_FULL_POS,   MAX31889_F_STATUS_A_FULL);
    stat.bits.temp_low  = GET_BIT_VAL(val8, MAX31889_F_STATUS_TEMP_LO_POS,  MAX31889_F_STATUS_TEMP_LO);
    stat.bits.temp_high = GET_BIT_VAL(val8, MAX31889_F_STATUS_TEMP_HI_POS,  MAX31889_F_STATUS_TEMP_HI);
    stat.bits.temp_rdy  = GET_BIT_VAL(val8, MAX31889_F_STATUS_TEMP_RDY_POS, MAX31889_F_STATUS_TEMP_RDY); 

    return ret;
}

int MAX31889::config_gpio(gpio_t gpio, gpio_mode_t mode)
{
    int ret = 0;
    uint8_t byt;

    if ( (gpio >= GPIO_NUM_MAX) || 
         (mode >= GPIO_MODE_MAX) ) {
        return -1; // invalid parameter
    }

    ret = read_register(MAX31889_R_GPIO_SETUP, &byt);
    if (ret) {
        return ret;
    }

    byt &= ~GPIO_MODE_MASK((int)gpio);
    byt |= GPIO_MODE_SET_MODE((int)gpio, (int)mode);

    ret = write_register(MAX31889_R_GPIO_SETUP, &byt);

    return ret;
}

int MAX31889::set_gpio_state(gpio_t gpio, int state)
{
    int ret = 0;
    uint8_t byt;

    if (gpio >= GPIO_NUM_MAX) {
        return -1; // invalid parameter
    }

    ret = read_register(MAX31889_R_GPIO_CTRL, &byt);
    if (ret) {
        return ret;
    }

    if (state) {
        byt |= GPIO_LOGIC_LEVEL_MASK((int)gpio);
    } else {
        byt &= ~GPIO_LOGIC_LEVEL_MASK((int)gpio); 
    }

    ret = write_register(MAX31889_R_GPIO_CTRL, &byt);

    return ret;
}

int MAX31889::get_gpio_state(gpio_t gpio)
{
    int ret = 0;
    uint8_t byt;

    if (gpio >= GPIO_NUM_MAX) {
        return -1; // invalid parameter
    }

    ret = read_register(MAX31889_R_GPIO_CTRL, &byt);
    if (ret) {
        return -1;
    }

    if ( byt & GPIO_LOGIC_LEVEL_MASK((int)gpio) ) {
        return 1; // state high
    } else {
        return 0; // state low
    }
}

int MAX31889::irq_enable(intr_id_t id)
{
    int ret = 0;
    uint8_t byt;
    uint8_t requested_irq = (uint8_t)id;

    // mask other bits
    requested_irq &= (uint8_t)(INTR_ID_TEMP_RDY | INTR_ID_TEMP_HIGH | INTR_ID_TEMP_LOW | INTR_ID_FIFO_ALMOST_FULL);

    ret = read_register(MAX31889_R_INT_EN, &byt);
    if (ret) {
        return ret;
    }

    byt |= requested_irq;   // enable interrupt
    ret = write_register(MAX31889_R_INT_EN, &byt);

    return ret;
}

int MAX31889::irq_disable(intr_id_t id)
{
    int ret = 0;
    uint8_t byt;
    uint8_t requested_irq = (uint8_t)id;

    // mask other bits
    requested_irq &= (uint8_t)(INTR_ID_TEMP_RDY | INTR_ID_TEMP_HIGH | INTR_ID_TEMP_LOW | INTR_ID_FIFO_ALMOST_FULL);

    ret = read_register(MAX31889_R_INT_EN, &byt);
    if (ret) {
        return ret;
    }

    byt &= ~requested_irq;  // disable interrupt
    ret = write_register(MAX31889_R_INT_EN, &byt);

    return ret;
}

int MAX31889::irq_clear_flag(intr_id_t id /*=INTR_ID_ALL*/)
{
    int ret = 0;
    uint8_t val8;

    // reading status register will clear flags
    ret = read_register(MAX31889_R_STATUS, &val8);

    return ret;
}

int MAX31889::set_alarm(float temp_low, float temp_high)
{
    int ret = 0;
    uint8_t buf[4];
    uint16_t count;

    count = convert_temp_2_count(temp_high);
    buf[0] = (uint8_t)(count>>8); //msb first
    buf[1] = (uint8_t)count;
    
    count = convert_temp_2_count(temp_low);
    buf[2] = (uint8_t)(count>>8); //msb first
    buf[3] = (uint8_t)count;

    ret = write_register(MAX31889_R_ALARM_HI_MSB, buf, 4);

    return ret;
}

int MAX31889::get_alarm(float &temp_low, float &temp_high)
{
    int ret = 0;
    uint8_t buf[4];
    uint16_t count;

    // Burst mode read, HIGH (MSB-LSB) and LOW (MSB-LSB) 
    ret = read_register(MAX31889_R_ALARM_HI_MSB, buf, 4);
    if (ret) {
        return ret;
    }
    //
    count = (buf[0]<<8) | buf[1];
    temp_high = convert_count_2_temp(count);
    
    //
    count = (buf[2]<<8) | buf[3];
    temp_low = convert_count_2_temp(count);

    return ret;
}

int MAX31889::get_id(id_t &id)
{
    int ret = 0;

    ret = read_register(MAX31889_R_ROM_ID_1, id.rom_id, 6);

    if (ret == 0) {
        ret = read_register(MAX31889_R_PART_IDF, &id.part_id); 
    }

    return ret;
}

int MAX31889::start_temp_conversion(void)
{
    int ret = 0;
    uint8_t byt;

    byt = MAX31889_F_TEMP_SENS_SETUP_CONVERT_T;
    ret = write_register(MAX31889_R_TEMP_SENS_SETUP, &byt);

    return ret;
}

int MAX31889::get_num_of_sample(void)
{
    int  ret = 0;
    uint8_t fifo_count = 0;
    uint8_t ovf_count = 0;
    int  num_of_samples = 0;

    ret = read_register(MAX31889_R_FIFO_DATA_COUNTER, &fifo_count);
    if (ret) {
        return -1;
    }

    ret = read_register(MAX31889_R_FIFO_OVF_CNT, &ovf_count);
    if (ret) {
        return -1;
    }

    if (ovf_count == 0) {
        num_of_samples = fifo_count & (MAX31889_FIFO_DEPTH-1);//mask with 0x1F
    } else {
        num_of_samples = MAX31889_FIFO_DEPTH;
    }

    return num_of_samples;
}

int MAX31889::get_temp(float *temp, int num_of_samples/*=1*/)
{
    int  ret = 0;
    uint8_t buf[2];

    for (int i = 0; i < num_of_samples; ++i) {

        ret = read_register(MAX31889_R_FIFO_DATA, buf, 2);
        if (ret) {
            return ret;
        }
        // 
        temp[i] = convert_count_2_temp( (buf[0]<<8) | buf[1] );
    }

    return ret;
}

int MAX31889::flush_fifo(void)
{
    int ret = 0;
    uint8_t val8;
    uint16_t count;

    ret = read_register(MAX31889_R_FIFO_CFG2, &val8);
    if (ret) {
        return ret;
    }

    val8 |= MAX31889_F_FIFO_CFG2_FLUSH_FIFO;
    ret = write_register(MAX31889_R_FIFO_CFG2, &val8);

    return ret;
}

int MAX31889::set_almost_full_depth(unsigned int num_of_samples)
{
    int ret = 0;
    uint8_t byt;

    if (num_of_samples >= MAX31889_FIFO_DEPTH) {
        return -1;
    }

    byt = MAX31889_FIFO_DEPTH - num_of_samples;
    ret = write_register(MAX31889_R_FIFO_CFG, &byt);

    return ret;
}

int MAX31889::get_fifo_configuration(reg_fifo_cfg_t &cfg)
{
    int ret = 0;
    uint8_t val8;

    ret = read_register(MAX31889_R_FIFO_CFG2, &val8);
    if (ret) {
        return ret;
    }

    cfg.bits.fifo_ro       = GET_BIT_VAL(val8, MAX31889_F_FIFO_CFG2_FIFO_RO_POS,       MAX31889_F_FIFO_CFG2_FIFO_RO);
    cfg.bits.a_full_type   = GET_BIT_VAL(val8, MAX31889_F_FIFO_CFG2_A_FULL_TYPE_POS,   MAX31889_F_FIFO_CFG2_A_FULL_TYPE);
    cfg.bits.fifo_stat_clr = GET_BIT_VAL(val8, MAX31889_F_FIFO_CFG2_FIFO_STAT_CLR_POS, MAX31889_F_FIFO_CFG2_FIFO_STAT_CLR); 
    cfg.bits.flush_fifo    = GET_BIT_VAL(val8, MAX31889_F_FIFO_CFG2_FLUSH_FIFO_POS,    MAX31889_F_FIFO_CFG2_FLUSH_FIFO); 

    return ret;
}

int MAX31889::set_fifo_configuration(reg_fifo_cfg_t cfg)
{
    int ret = 0;
    uint8_t val8 = 0;

    val8 |= SET_BIT_VAL(cfg.bits.fifo_ro ,      MAX31889_F_FIFO_CFG2_FIFO_RO_POS,       MAX31889_F_FIFO_CFG2_FIFO_RO);
    val8 |= SET_BIT_VAL(cfg.bits.a_full_type,   MAX31889_F_FIFO_CFG2_A_FULL_TYPE_POS,   MAX31889_F_FIFO_CFG2_A_FULL_TYPE);
    val8 |= SET_BIT_VAL(cfg.bits.fifo_stat_clr, MAX31889_F_FIFO_CFG2_FIFO_STAT_CLR_POS, MAX31889_F_FIFO_CFG2_FIFO_STAT_CLR);
    val8 |= SET_BIT_VAL(cfg.bits.flush_fifo,    MAX31889_F_FIFO_CFG2_FLUSH_FIFO_POS,    MAX31889_F_FIFO_CFG2_FLUSH_FIFO);

    ret = write_register(MAX31889_R_FIFO_CFG2, &val8);

    return ret;
}
