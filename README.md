# MAX17320 Dev Tools
## Building
  `make build`

## Usage
dump all registers to json
```sudo ./max17320_read```

query specific registers by address or name (case insensitive)
e.g.
```sudo ./max17320_read Status VCell 0x05```

## Pipe register list into app
```cat /path/to/register/list | xarg sudo ./max17320_read```

## CSV output
add `--csv` to poll the selected registers continuously, emitting a header row
of register names followed by one row of values per reading. output is
flushed per row so it can be tailed or appended to a log file
```sudo ./max17320_read --csv Current Power >> log.csv```

units are named once in the header rather than repeated on every value
```
Current (mA),Power (mW)
-125.000000,-924.800000
```

polling runs at 1 Hz until interrupted with ctrl-c. use `--interval` to set a
different period in seconds (fractions allowed)
```sudo ./max17320_read --csv --interval 0.5 Current Power```

the flags can appear anywhere in the argument list, and work with a piped
register list too
```cat /path/to/register/list | xargs sudo ./max17320_read --csv```

## Clearing recorded current peaks
`--clear-max-current` resets the MaxMinCurr register to its power-on value
before the first reading, so a run reports only the current seen while it was
running
```sudo ./max17320_read --clear-max-current --csv Current MaxMinCurr```

this clears the gauge's write protection and leaves it cleared, matching what
the rest of the driver does — re-locking would write 0x00F9 to CommStat and
re-enable both FETs

## Nonvolatile memory
the expected nonvolatile settings are the `g_nv_expected` table in
`src/bms_ctl.c`, kept in step with `whale-tag-stm32/src/battery/bms_ctl.c`.
select the board revision and cells with `BMS_VERSION` and `BMS_CELLS`

compare the pack's nonvolatile settings against the table, exiting nonzero on
any mismatch
```sudo ./max17320_read --verify-nv```

program the table into nonvolatile memory
```sudo ./max17320_read --program-nv```

the MAX17320 only allows a limited number of nonvolatile updates, so
programming
- is skipped when the pack already holds the expected values
- is refused when fewer than 3 updates remain
- writes shadow RAM and reads it back before committing anything
- takes about 8 s for the block copy, then resets the gauge and verifies the
  stored values
- re-enables write protection afterwards, as the STM32 firmware does

## Shutdown
open both the charge and discharge FETs, cutting current through the pack
```sudo ./max17320_read --shutdown```

both FETs are opened in a single CommStat write, so a host powered through the
pack loses power right after the write and never prints the confirmation. on a
host powered separately, CommStat is read back to confirm both FETs are open.

this only takes effect when comm override is enabled in `nProtCfg` (CmOvrdEn).
write protection is cleared and left cleared. note that clearing or re-locking
write protection rewrites CommStat and closes both FETs again, so any later
command that does so (e.g. `--clear-max-current`, `--program-nv`) reconnects
the pack
