program iotest
use mpi
real, dimension(100) :: x
integer :: mpierr
integer :: rank
integer :: i

call mpi_init(mpierr)
do i = 1,100
  x(i) = i / 3.0
end do
call mpi_comm_rank(MPI_COMM_WORLD, rank, ierr)
if(rank .eq. 0) then
  print *, "writing..."
  open(12, file='.fortran-output', action="write", access='stream', &
       form='unformatted')
  write(12) x
  close(12)
endif
print *, 'finished.'
call mpi_finalize(ierr)
end program iotest
