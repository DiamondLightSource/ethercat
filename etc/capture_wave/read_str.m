function str = read_str(file)
  buflen = fread(file, 1, 'int32');
  buffer = fread(file, buflen, 'schar');
  str = char(buffer');
