function PetscBinaryWrite(inarg,varargin)
%
%  Writes in PETSc binary file sparse matrices and vectors
%  if the array is multidimensional and dense it is saved
%  as a one dimensional array
%
%   inarg may be:
%      filename 
%      socket number (0 for PETSc default)
%      the object returned from sreader() or freader()
%
if ischar(inarg) 
  fd = freader(inarg,'w');
else if isnumeric(inarg)
  if inarg == 0
    fd = sreader;
  else 
    fd = sreader(inarg);
  end
else 
  fd = inarg;
end
end

for l=1:nargin-1
  A = varargin{l}; 
  if issparse(A)
    % save sparse matrix in special Matlab format
    [m,n] = size(A);
    majic = 1.2345678910e-30;
    for i=1:min(m,n)
      if A(i,i) == 0
        A(i,i) = majic;
      end
    end
    if min(size(A)) == 1     %a one-rank matrix will be compressed to a
                             %scalar instead of a vectory by sum
      n_nz = full(A' ~= 0);
    else
      n_nz = full(sum(A' ~= 0));
    end
    nz   = sum(n_nz);
    write(fd,[1211216,m,n,nz],'int32');

    write(fd,n_nz,'int32');  %nonzeros per row
    [i,j,s] = find((A' ~= 0).*(A'));
    write(fd,i-1,'int32');
    for i=1:nz
      if s(i) == majic
        s(i) = 0;
      end
    end
    write(fd,s,'double');
  else
    [m,n] = size(A);
    write(fd,[1211214,m*n],'int32');
    write(fd,A,'double');
  end
end
if ischar(inarg) | isinteger(inarg) close(fd); end;
