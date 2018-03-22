R"(
define @ielert.memory.allocate(%size) {
entry:
  // get next free address
  // note that the address 0 of memory is reserved for a pointer to
  // the last allocated memory location
  %last.allocd = load 0
  %ptr = add %last.allocd, 1

  // compute new last allocd address
  %last.allocd = add %last.allocd, 1
  %last.allocd = add %last.allocd, %size

  // allocate
  store %last.allocd, 0

  // initialize the header
  store %size, %ptr // size of chunk

  // return the address of the header
  ret %ptr
}

define @ielert.memory.size(%ptr) {
entry:
  %size = load %ptr
  ret %size
}

define @ielert.memory.address(%ptr, %index) {
entry:
  // check for out-of-bounds access
  %size = load %ptr
  %out.of.bounds = cmp ge %index, %size
  br %out.of.bounds, throw

  // return the address
  %ptr1 = add %ptr, 1
  %addr = add %ptr1, %index
  ret %addr

throw:
  revert -1
}

define @ielert.memory.load(%ptr, %index) {
entry:
  // get the address
  %addr = call @ielert.memory.address(%ptr, %index)

  // load and return the value
  %val = load %addr
  ret %val
})"
