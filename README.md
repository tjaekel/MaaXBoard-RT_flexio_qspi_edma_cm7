# MaaXBoard-RT_flexio_qspi_edma_cm7
 QSPI with FLEXIO2 and EDMA

## QSPI with FLEXIO
This is a demo (based on SDK examples) how to implement a QSPI with FLEXIO.

## Remarks
ATT: the driver files, e.g. "fsl_flexio_spi.c" need a modification for QSPI!
(modified files in project)

QSPI Write plus a QSPI Read works:
Just to fix:
the PCS (nSS) signal toggles between both transactions:
we have to find a way to keep PCS low betweeen a write and a read, e.g. use a software GPIO pin output.

It works up to 30 MHz QSPI SCLK (60 MHz might be wrong waveform).

## How does it work?
I have taken a FLEXIO SPI example and extended/modified for QSPI:
output 4 bits in parallel (write) or receives 4 bits in parallel (read)
Connstraints are:
* have 4 consecutive GPIO pins available - we have, just four consecutive for D0..D3
* use the "PARALLEL" mode (WIDTH on shifter config)
* modify the SCLK timer to generate 8 pulses for a 32bit word (and EDMA trigger)

It works, as QSPI write and a following read.:
It flips the direction of the D0..D3 signals (use the same consecutive pins, but on a different SHIFTER).
Fix the issue with PCS de-asserted between both transactions (e.g. via SW GPIO pin).

