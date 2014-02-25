program iotest
implicit none
real, dimension(100) :: x
integer :: i

do i = 1,100
  x(i) = i / 3.0
end do
open(12, file='.fortran-output', action="write", access='stream', &
     form='unformatted')
write(12) x
close(12)
print *, 'finished.'
end program iotest
