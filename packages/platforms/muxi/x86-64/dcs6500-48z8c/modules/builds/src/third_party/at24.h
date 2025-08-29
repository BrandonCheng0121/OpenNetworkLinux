#ifndef __AT24_H
#define __AT24_H

#define MAX_EEPROM_NUM  (10)
ssize_t at24_read_eeprom(unsigned int adapter_nr, unsigned int addr, uint8_t *eeprom, unsigned int offset, unsigned int count);

#endif /* AT24_H */