SECTIONS
{
  .sdram (NOLOAD) : ALIGN(4) SUBALIGN(4)
  {
    . = ALIGN(4);
    _usr_sdram_start = .;
    KEEP(*(.sdram*))
    . = ALIGN(4);
    _usr_sdram_end = .;
  } > SDRAM
}
