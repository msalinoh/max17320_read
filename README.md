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
