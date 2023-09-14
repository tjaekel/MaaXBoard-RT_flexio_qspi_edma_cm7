# MaaXBoard-RT_flexio_qspi_edma_cm7
 QSPI with FLEXIO2 and EDMA

## QSPI with FLEXIO
This is a demo (based on SDK examples) how to implement a QSPI with FLEXIO.

## Remarks
ATT: the driver files, e.g. "fsl_flexio_spi.c" need a modification for QSPI!
(modified files in project)

Currently, just the QSPI Tx is implemented.
For Rx (based on reverse the D0..D3 direction) - it needs still implementation.

It works up to 30 MHz QSPI SCLK (60 might be wrong waveform).

## How does it work?
I have taken a FLEXIO SPI example and extended/modified for QSPI:
output 4 bits in parallel.
Connstraints are:
* have 4 consecutive GPIO pins available
* use the "PARALLEL" mode
* modify the SCLK timer to generate 4 pulses for a 32bit word (and EDMA trigger)

It works, as QSPI write. Just to add QSPI read (similar, just flip the direction of the D0..D3 signals).

