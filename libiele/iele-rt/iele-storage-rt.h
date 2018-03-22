R"(
define @ielert.storage.allocate(%size) {
entry:
  // get next free address
  %ptr = sload @ielert.storage.next

  // compute new next free address
  %next = add %ptr, 2
  %next = add %next, %size

  // allocate
  sstore %next, @ielert.storage.next

  // initialize the header
  sstore %size, %ptr // size of chunk
  %ptr1 = add %ptr, 1
  sstore 0, %ptr1    // pointer to next chunk, initially null

  // return the address of the header
  ret %ptr
}

define @ielert.storage.size(%ptr) {
entry:
  %size = sload %ptr
  %ptr1 = add %ptr, 1
  %chunk.ptr = sload %ptr1

  // loop over chunks to accumulate the size
loop:
  %done = cmp eq %chunk.ptr, 0
  br %done, exit.loop
  // 1. accumulate chunk's size
  %chunk.size = sload %chunk.ptr
  %size = add %size, %chunk.size
  // 2. move to next chunk
  %chunk.ptr1 = add %chunk.ptr, 1
  %chunk.ptr = sload %chunk.ptr1

exit.loop:
  ret %size
}

define @ielert.storage.address(%ptr, %index) {
entry:
  %ptr.chunk = %ptr
  %index.chunk = %index

chunk.lookup:
  // find chunk

  // 1. check if this is the correct chunk
  %size.chunk = sload %ptr.chunk
  %found = cmp lt %index.chunk, %size.chunk
  br %found, chunk.found

  // 2. if it's not, move to the next one
  %ptr.chunk1 = add %ptr.chunk, 1
  %ptr.chunk = sload %ptr.chunk1

  // 2.1 check for out of bounds
  %out.of.bounds = iszero %ptr.chunk
  br %out.of.bounds, throw

  %index.chunk = sub %index.chunk, %size.chunk
  br chunk.lookup

chunk.found:
  // 3. return the address
  %ptr.chunk2 = add %ptr.chunk, 2
  %addr = add %ptr.chunk2, %index.chunk
  ret %addr

throw:
  revert -1
}

define @ielert.storage.load(%ptr, %index) {
entry:
  // get the address
  %addr = call @ielert.storage.address(%ptr, %index)

  // load and return the value
  %val = sload %addr
  ret %val
})"
