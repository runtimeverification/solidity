R"(
define @ielert.storage.allocate(%size) {
entry:
  // get next free address
  %next.free.ptr = sload @ielert.storage.next.free

  // compute new next free address
  %new.next.free.ptr = add %next.free.ptr, %size

  // allocate
  sstore %new.next.free.ptr, @ielert.storage.next.free

  // return the address of the first slot of newly allocated storage
  ret %next.free.ptr
})"
