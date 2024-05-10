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
