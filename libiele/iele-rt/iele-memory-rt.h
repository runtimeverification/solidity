R"(
define @ielert.memory.allocate(%size) {
entry:
  // get next free address
  // note that the address 0 of memory is reserved for a pointer to
  // the last allocated memory location
  %last.allocd = load 0
  %ptr = add %last.allocd, 1

  // compute new last allocd address
  %last.allocd = add %last.allocd, %size

  // allocate
  store %last.allocd, 0

  // return the address of the header
  ret %ptr
}

define @ielert.memory.fill(%to, %size, %data) {
fill.loop:
  %done = iszero %size
  br %done, exit
  store %data, %to
  %to = add %to, 1
  %size = sub %size, 1
  br fill.loop

exit:
  ret void
}

define @ielert.storage.fill(%to, %size, %data) {
fill.loop:
  %done = iszero %size
  br %done, exit
  sstore %data, %to
  %to = add %to, 1
  %size = sub %size, 1
  br fill.loop

exit:
  ret void
}

define @ielert.memory.copy.to.memory(%from, %to, %size) {
copy.loop:
  %done = iszero %size
  br %done, exit
  %data = load %from
  store %data, %to
  %from = add %from, 1
  %to = add %to, 1
  %size = sub %size, 1
  br copy.loop

exit:
  ret void
}

define @ielert.memory.copy.to.storage(%from, %to, %size) {
copy.loop:
  %done = iszero %size
  br %done, exit
  %data = load %from
  sstore %data, %to
  %from = add %from, 1
  %to = add %to, 1
  %size = sub %size, 1
  br copy.loop

exit:
  ret void
}

define @ielert.storage.copy.to.memory(%from, %to, %size) {
copy.loop:
  %done = iszero %size
  br %done, exit
  %data = sload %from
  store %data, %to
  %from = add %from, 1
  %to = add %to, 1
  %size = sub %size, 1
  br copy.loop

exit:
  ret void
}

define @ielert.storage.copy.to.storage(%from, %to, %size) {
copy.loop:
  %done = iszero %size
  br %done, exit
  %data = sload %from
  sstore %data, %to
  %from = add %from, 1
  %to = add %to, 1
  %size = sub %size, 1
  br copy.loop

exit:
  ret void
})"
