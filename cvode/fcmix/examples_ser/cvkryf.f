C File cvkryf.f
C
C FCVODE Example Problem: 2D kinetics-transport, precond. Krylov solver. 
C Version of 1 August 2003
C
C An ODE system is generated from the following 2-species diurnal
C kinetics advection-diffusion PDE system in 2 space dimensions:
C
C dc(i)/dt = Kh*(d/dx)**2 c(i) + V*dc(i)/dx + (d/dz)(Kv(z)*dc(i)/dz)
C                 + Ri(c1,c2,t)      for i = 1,2,   where
C   R1(c1,c2,t) = -q1*c1*c3 - q2*c1*c2 + 2*q3(t)*c3 + q4(t)*c2 ,
C   R2(c1,c2,t) =  q1*c1*c3 - q2*c1*c2 - q4(t)*c2 ,
C   Kv(z) = Kv0*exp(z/5) ,
C Kh, V, Kv0, q1, q2, and c3 are constants, and q3(t) and q4(t)
C vary diurnally.   The problem is posed on the square
C   0 .le. x .le. 20,    30 .le. z .le. 50   (all in km),
C with homogeneous Neumann boundary conditions, and for time t in
C   0 .le. t .le. 86400 sec (1 day).
C The PDE system is treated by central differences on a uniform
C 10 x 10 mesh, with simple polynomial initial profiles.
C The problem is solved with CVODE, with the BDF/GMRES method and
C the block-diagonal part of the Jacobian as a left preconditioner.
C
C Note: this program requires the dense linear solver routines
C DGEFA and DGESL from LINPACK, and BLAS routines DCOPY and DSCAL.
C
C The second and third dimensions of Y here must match the values of
C MESHX and MESHY, for consistency with the output statements below.
      DOUBLE PRECISION ATOL, AVDIM, DELT, FLOOR, RTOL, T, TOUT, TWOHR
      DOUBLE PRECISION Y(2,10,10), ROPT(40)
      INTEGER IOPT(40)
      DATA TWOHR/7200.0D0/, RTOL/1.0D-5/, FLOOR/100.0D0/,
     1     JPRETYPE/1/, IGSTYPE/0/, MAXL/0/, DELT/0.0D0/
      DATA LNST/4/, LNFE/5/, LNSETUP/6/, LNNI/7/, LNCF/8/,
     1     LQ/11/, LH/5/, LNPE/16/, LNLI/17/, LNPS/18/, LNCFL/19/
      INTEGER NEQ
      COMMON /PBDIM/ NEQ
C
C Set mesh sizes
      MESHX = 10
      MESHZ = 10
C Load Common and initial values in Subroutine INITKX
      CALL INITKX (MESHX, MESHZ, Y)
C Set other input arguments.
      NEQ = 2*MESHX*MESHZ
      T = 0.0D0
      METH = 2
      ITMETH = 2
      IATOL = 1
      ATOL = RTOL*FLOOR
      INOPT = 0
      ITASK = 0
C
      WRITE(6,10)NEQ
 10   FORMAT('Krylov example problem: Kinetics-transport, NEQ = ',I4/)
C
      CALL FNVSPECINITS(NEQ, IER)
      IF (IER .NE. 0) THEN
        WRITE(6,20) IER
 20     FORMAT(///' FMENVINITS returned IER =',I5)
        STOP
      ENDIF
C
      CALL FCVMALLOC (T, Y, METH, ITMETH, IATOL, RTOL, ATOL,
     1                INOPT, IOPT, ROPT, IER)
      IF (IER .NE. 0) THEN
        WRITE(6,30) IER
  30    FORMAT(///' FCVMALLOC returned IER =',I5)
        STOP
        ENDIF
C
      CALL FCVSPGMR (JPRETYPE, IGSTYPE, MAXL, DELT, IER)
      IF (IER .NE. 0) THEN
        WRITE(6,40) IER
 40     FORMAT(///' FCVSPGMR returned IER =',I5)
        STOP
      ENDIF

      CALL FCVSPGMRSETPSOL (1, IER)
      CALL FCVSPGMRSETPRECO (1, IER)
C
C Loop over output points, call FCVODE, print sample solution values.
      TOUT = TWOHR
      DO 70 IOUT = 1,12
C
        CALL FCVODE (TOUT, T, Y, ITASK, IER)
C
        WRITE(6,50) T,IOPT(LNST),IOPT(LQ),ROPT(LH)
 50     FORMAT(/' t =',D10.2,5X,'no. steps =',I5,
     1                     '   order =',I3,'   stepsize =',D10.2)
        WRITE(6,55) Y(1,1,1), Y(1,5,5), Y(1,10,10),
     1              Y(2,1,1), Y(2,5,5), Y(2,10,10)
 55     FORMAT('  c1 (bot.left/middle/top rt.) =',3D12.3/
     1         '  c2 (bot.left/middle/top rt.) =',3D12.3)
C
        IF (IER .NE. 0) THEN
          WRITE(6,60) IER
 60       FORMAT(///' FCVODE returned IER =',I5)
          STOP
          ENDIF
C
        TOUT = TOUT + TWOHR
 70     CONTINUE

C Print final statistics.
      NST = IOPT(LNST)
      NFE = IOPT(LNFE)
      NPSET = IOPT(LNSETUP)
      NPE = IOPT(LNPE)
      NPS = IOPT(LNPS)
      NNI = IOPT(LNNI)
      NLI = IOPT(LNLI)
      AVDIM = REAL(NLI)/REAL(NNI)
      NCFN = IOPT(LNCF)
      NCFL = IOPT(LNCFL)
      WRITE (6,80) NST,NFE,NPSET,NPE,NPS,NNI,NLI,AVDIM,NCFN,NCFL
  80  FORMAT(//' final statistics..'/
     1 ' number of steps        =',I5,5X,'number of f evals.     =',I5/
     2 ' number of prec. setups =',I5/
     3 ' number of prec. evals. =',I5,5X,'number of prec. solves =',I5/
     4 ' number of nonl. iters. =',I5,5X,'number of lin. iters.  =',I5/
     5 ' average Krylov subspace dimension (NLI/NNI)  =',F8.4/
     6 ' number of conv. failures..  nonlinear =',I3,'  linear =',I3)
C
      CALL FCVFREE
      CALL FNVSPECFREES
C
      STOP
      END

      SUBROUTINE INITKX (MESHX, MESHZ, Y0)
C Routine to set problem constants and initial values
      DIMENSION Y0(2, MESHX, MESHZ)
      DOUBLE PRECISION Q1,Q2,Q3,Q4, A3,A4, OM, C3, DZ, HDCO,VDCO,HACO
      COMMON /PCOM/ Q1,Q2,Q3,Q4,A3,A4,OM,C3,DZ,HDCO,VDCO,HACO,MX,MZ,MM
      DOUBLE PRECISION CX, CZ, DKH, DKV0, DX, HALFDA, PI, VEL, 
     1                 X, Y0, Z
      DATA DKH/4.0D-6/, VEL/0.001D0/, DKV0/1.0D-8/, HALFDA/4.32D4/,
     1  PI/3.1415926535898D0/
C
C Load Common block of problem parameters.
      MX = MESHX
      MZ = MESHZ
      MM = MX*MZ
      Q1 = 1.63D-16
      Q2 = 4.66D-16
      A3 = 22.62D0
      A4 = 7.601D0
      OM = PI/HALFDA
      C3 = 3.7D16
      DX = 20.0D0/(MX - 1.0D0)
      DZ = 20.0D0/(MZ - 1.0D0)
      HDCO = DKH/DX**2
      HACO = VEL/(2.0D0*DX)
      VDCO = (1.0D0/DZ**2)*DKV0
C
C Set initial profiles.
      DO 20 JZ = 1,MZ
        Z = 30.0D0 + (JZ - 1.0D0)*DZ
        CZ = (0.1D0*(Z - 40.0D0))**2
        CZ = 1.0D0 - CZ + 0.5D0*CZ**2
        DO 10 JX = 1,MX
          X = (JX - 1.0D0)*DX
          CX = (0.1D0*(X - 10.0D0))**2
          CX = 1.0D0 - CX + 0.5D0*CX**2
          Y0(1,JX,JZ) = 1.0D6*CX*CZ
          Y0(2,JX,JZ) = 1.0D12*CX*CZ
 10       CONTINUE
 20     CONTINUE
C
      RETURN
      END

      SUBROUTINE CVFUN (T, Y, YDOT)
C Routine for right-hand side function f
      DOUBLE PRECISION T, Y(2,*), YDOT(2,*)
      DOUBLE PRECISION Q1,Q2,Q3,Q4, A3,A4, OM, C3, DZ, HDCO,VDCO,HACO
      COMMON /PCOM/ Q1,Q2,Q3,Q4,A3,A4,OM,C3,DZ,HDCO,VDCO,HACO,MX,MZ,MM
      DOUBLE PRECISION C1, C2, C1DN, C2DN, C1UP, C2UP, C1LT, C2LT,
     1    C1RT, C2RT, CZDN, CZUP, HORD1, HORD2, HORAD1, HORAD2,
     2    QQ1, QQ2, QQ3, QQ4, RKIN1, RKIN2, S, VERTD1, VERTD2, ZDN, ZUP
C
C Set diurnal rate coefficients.
      S = SIN(OM*T)
      IF (S .GT. 0.0D0) THEN
        Q3 = EXP(-A3/S)
        Q4 = EXP(-A4/S)
      ELSE
        Q3 = 0.0D0
        Q4 = 0.0D0
      ENDIF
C
C Loop over all grid points.
      DO 20 JZ = 1,MZ
        ZDN = 30.0D0 + (JZ - 1.5D0)*DZ
        ZUP = ZDN + DZ
        CZDN = VDCO*EXP(0.2D0*ZDN)
        CZUP = VDCO*EXP(0.2D0*ZUP)
        IBLOK0 = (JZ-1)*MX
        IDN = -MX
        IF (JZ .EQ. 1) IDN = MX
        IUP = MX
        IF (JZ .EQ. MZ) IUP = -MX
        DO 10 JX = 1,MX
          IBLOK = IBLOK0 + JX
          C1 = Y(1,IBLOK)
          C2 = Y(2,IBLOK)
C Set kinetic rate terms.
          QQ1 = Q1*C1*C3
          QQ2 = Q2*C1*C2
          QQ3 = Q3*C3
          QQ4 = Q4*C2
          RKIN1 = -QQ1 - QQ2 + 2.0D0*QQ3 + QQ4
          RKIN2 = QQ1 - QQ2 - QQ4
C Set vertical diffusion terms.
          C1DN = Y(1,IBLOK+IDN)
          C2DN = Y(2,IBLOK+IDN)
          C1UP = Y(1,IBLOK+IUP)
          C2UP = Y(2,IBLOK+IUP)
          VERTD1 = CZUP*(C1UP - C1) - CZDN*(C1 - C1DN)
          VERTD2 = CZUP*(C2UP - C2) - CZDN*(C2 - C2DN)
C Set horizontal diffusion and advection terms.
          ILEFT = -1
          IF (JX .EQ. 1) ILEFT = 1
          IRIGHT = 1
          IF (JX .EQ. MX) IRIGHT = -1
          C1LT = Y(1,IBLOK+ILEFT)
          C2LT = Y(2,IBLOK+ILEFT)
          C1RT = Y(1,IBLOK+IRIGHT)
          C2RT = Y(2,IBLOK+IRIGHT)
          HORD1 = HDCO*(C1RT - 2.0D0*C1 + C1LT)
          HORD2 = HDCO*(C2RT - 2.0D0*C2 + C2LT)
          HORAD1 = HACO*(C1RT - C1LT)
          HORAD2 = HACO*(C2RT - C2LT)
C Load all terms into YDOT.
          YDOT(1,IBLOK) = VERTD1 + HORD1 + HORAD1 + RKIN1
          YDOT(2,IBLOK) = VERTD2 + HORD2 + HORAD2 + RKIN2
 10       CONTINUE
 20     CONTINUE
      RETURN
      END

      SUBROUTINE CVPRECO (T, Y, FY, JOK, JCUR, GAMMA, EWT, H, UR,
     1                    V1, V2, V3, IER)
C Routine to set and preprocess block-diagonal preconditioner.
C Note: The dimensions in /BDJ/ below assume at most 100 mesh points.
      DOUBLE PRECISION T, Y(2,*), GAMMA
      DOUBLE PRECISION Q1,Q2,Q3,Q4, A3,A4, OM, C3, DZ, HDCO,VDCO,HACO
      COMMON /PCOM/ Q1,Q2,Q3,Q4,A3,A4,OM,C3,DZ,HDCO,VDCO,HACO,MX,MZ,MM
      DOUBLE PRECISION BD, P
      COMMON /BDJ/ BD(2,2,100), P(2,2,100), IPP(2,100)
      DOUBLE PRECISION C1, C2, CZDN, CZUP, DIAG, TEMP, ZDN, ZUP
C
      IER = 0
      LENBD = 4*MM
C
C If JOK = 1, copy BD to P.
      IF (JOK .EQ. 1) THEN
        CALL DCOPY (LENBD, BD(1,1,1), 1, P(1,1,1), 1)
        JCUR = 0
      ELSE
C
C JOK = 0.  Compute diagonal Jacobian blocks and copy to P.
C   (using q4 value computed on last FFUN call).
      DO 20 JZ = 1,MZ
        ZDN = 30.0D0 + (JZ - 1.5D0)*DZ
        ZUP = ZDN + DZ
        CZDN = VDCO*EXP(0.2D0*ZDN)
        CZUP = VDCO*EXP(0.2D0*ZUP)
        DIAG = -(CZDN + CZUP + 2.0D0*HDCO)
        IBLOK0 = (JZ-1)*MX
        DO 10 JX = 1,MX
          IBLOK = IBLOK0 + JX
          C1 = Y(1,IBLOK)
          C2 = Y(2,IBLOK)
          BD(1,1,IBLOK) = (-Q1*C3 - Q2*C2) + DIAG
          BD(1,2,IBLOK) = -Q2*C1 + Q4
          BD(2,1,IBLOK) =  Q1*C3 - Q2*C2
          BD(2,2,IBLOK) = (-Q2*C1 - Q4) + DIAG
 10       CONTINUE
 20     CONTINUE
      CALL DCOPY (LENBD, BD(1,1,1), 1, P(1,1,1), 1)
      JCUR = 1
      ENDIF
C
C Scale P by -GAMMA.
      TEMP = -GAMMA
      CALL DSCAL (LENBD, TEMP, P, 1)
C
C Add identity matrix and do LU decompositions on blocks, in place.
      DO 40 IBLOK = 1,MM
        P(1,1,IBLOK) = P(1,1,IBLOK) + 1.0D0
        P(2,2,IBLOK) = P(2,2,IBLOK) + 1.0D0
        CALL DGEFA (P(1,1,IBLOK), 2, 2, IPP(1,IBLOK), IER)
        IF (IER .NE. 0) RETURN
 40     CONTINUE
C
      RETURN
      END

      SUBROUTINE CVPSOL (T, Y, FY, VTEMP, GAMMA, EWT, DELTA,
     1                   R, LR, Z, IER)
C Routine to solve preconditioner linear system.
C Note: The dimensions in /BDJ/ below assume at most 100 mesh points.
      DOUBLE PRECISION R(*), Z(2,*)
      DOUBLE PRECISION Q1,Q2,Q3,Q4, A3,A4, OM, C3, DZ, HDCO,VDCO,HACO
      COMMON /PCOM/ Q1,Q2,Q3,Q4,A3,A4,OM,C3,DZ,HDCO,VDCO,HACO,MX,MZ,MM
      DOUBLE PRECISION BD, P
      COMMON /BDJ/ BD(2,2,100), P(2,2,100), IPP(2,100)
      INTEGER NEQ
      COMMON /PBDIM/ NEQ
C Solve the block-diagonal system Px = r using LU factors stored in P
C and pivot data in IPP, and return the solution in Z.
      IER = 0
      CALL DCOPY (NEQ, R, 1, Z, 1)
      DO 10 I = 1,MM
        CALL DGESL (P(1,1,I), 2, 2, IPP(1,I), Z(1,I), 0)
 10     CONTINUE
      RETURN
      END

      subroutine dgefa(a,lda,n,ipvt,info)
      integer lda,n,ipvt(1),info
      double precision a(lda,1)
c
c     dgefa factors a double precision matrix by gaussian elimination.
c
c     dgefa is usually called by dgeco, but it can be called
c     directly with a saving in time if  rcond  is not needed.
c     (time for dgeco) = (1 + 9/n)*(time for dgefa) .
c
c     on entry
c
c        a       double precision(lda, n)
c                the matrix to be factored.
c
c        lda     integer
c                the leading dimension of the array  a .
c
c        n       integer
c                the order of the matrix  a .
c
c     on return
c
c        a       an upper triangular matrix and the multipliers
c                which were used to obtain it.
c                the factorization can be written  a = l*u  where
c                l  is a product of permutation and unit lower
c                triangular matrices and  u  is upper triangular.
c
c        ipvt    integer(n)
c                an integer vector of pivot indices.
c
c        info    integer
c                = 0  normal value.
c                = k  if  u(k,k) .eq. 0.0 .  this is not an error
c                     condition for this subroutine, but it does
c                     indicate that dgesl or dgedi will divide by zero
c                     if called.  use  rcond  in dgeco for a reliable
c                     indication of singularity.
c
c     linpack. this version dated 08/14/78 .
c     cleve moler, university of new mexico, argonne national lab.
c
c     subroutines and functions
c
c     blas daxpy,dscal,idamax
c
c     internal variables
c
      double precision t
      integer idamax,j,k,kp1,l,nm1
c
c
c     gaussian elimination with partial pivoting
c
      info = 0
      nm1 = n - 1
      if (nm1 .lt. 1) go to 70
      do 60 k = 1, nm1
         kp1 = k + 1
c
c        find l = pivot index
c
         l = idamax(n-k+1,a(k,k),1) + k - 1
         ipvt(k) = l
c
c        zero pivot implies this column already triangularized
c
         if (a(l,k) .eq. 0.0d0) go to 40
c
c           interchange if necessary
c
            if (l .eq. k) go to 10
               t = a(l,k)
               a(l,k) = a(k,k)
               a(k,k) = t
   10       continue
c
c           compute multipliers
c
            t = -1.0d0/a(k,k)
            call dscal(n-k,t,a(k+1,k),1)
c
c           row elimination with column indexing
c
            do 30 j = kp1, n
               t = a(l,j)
               if (l .eq. k) go to 20
                  a(l,j) = a(k,j)
                  a(k,j) = t
   20          continue
               call daxpy(n-k,t,a(k+1,k),1,a(k+1,j),1)
   30       continue
         go to 50
   40    continue
            info = k
   50    continue
   60 continue
   70 continue
      ipvt(n) = n
      if (a(n,n) .eq. 0.0d0) info = n
      return
      end
      subroutine dgesl(a,lda,n,ipvt,b,job)
      integer lda,n,ipvt(1),job
      double precision a(lda,1),b(1)
c
c     dgesl solves the double precision system
c     a * x = b  or  trans(a) * x = b
c     using the factors computed by dgeco or dgefa.
c
c     on entry
c
c        a       double precision(lda, n)
c                the output from dgeco or dgefa.
c
c        lda     integer
c                the leading dimension of the array  a .
c
c        n       integer
c                the order of the matrix  a .
c
c        ipvt    integer(n)
c                the pivot vector from dgeco or dgefa.
c
c        b       double precision(n)
c                the right hand side vector.
c
c        job     integer
c                = 0         to solve  a*x = b ,
c                = nonzero   to solve  trans(a)*x = b  where
c                            trans(a)  is the transpose.
c
c     on return
c
c        b       the solution vector  x .
c
c     error condition
c
c        a division by zero will occur if the input factor contains a
c        zero on the diagonal.  technically this indicates singularity
c        but it is often caused by improper arguments or improper
c        setting of lda .  it will not occur if the subroutines are
c        called correctly and if dgeco has set rcond .gt. 0.0
c        or dgefa has set info .eq. 0 .
c
c     to compute  inverse(a) * c  where  c  is a matrix
c     with  p  columns
c           call dgeco(a,lda,n,ipvt,rcond,z)
c           if (rcond is too small) go to ...
c           do 10 j = 1, p
c              call dgesl(a,lda,n,ipvt,c(1,j),0)
c        10 continue
c
c     linpack. this version dated 08/14/78 .
c     cleve moler, university of new mexico, argonne national lab.
c
c     subroutines and functions
c
c     blas daxpy,ddot
c
c     internal variables
c
      double precision ddot,t
      integer k,kb,l,nm1
c
      nm1 = n - 1
      if (job .ne. 0) go to 50
c
c        job = 0 , solve  a * x = b
c        first solve  l*y = b
c
         if (nm1 .lt. 1) go to 30
         do 20 k = 1, nm1
            l = ipvt(k)
            t = b(l)
            if (l .eq. k) go to 10
               b(l) = b(k)
               b(k) = t
   10       continue
            call daxpy(n-k,t,a(k+1,k),1,b(k+1),1)
   20    continue
   30    continue
c
c        now solve  u*x = y
c
         do 40 kb = 1, n
            k = n + 1 - kb
            b(k) = b(k)/a(k,k)
            t = -b(k)
            call daxpy(k-1,t,a(1,k),1,b(1),1)
   40    continue
      go to 100
   50 continue
c
c        job = nonzero, solve  trans(a) * x = b
c        first solve  trans(u)*y = b
c
         do 60 k = 1, n
            t = ddot(k-1,a(1,k),1,b(1),1)
            b(k) = (b(k) - t)/a(k,k)
   60    continue
c
c        now solve trans(l)*x = y
c
         if (nm1 .lt. 1) go to 90
         do 80 kb = 1, nm1
            k = n - kb
            b(k) = b(k) + ddot(n-k,a(k+1,k),1,b(k+1),1)
            l = ipvt(k)
            if (l .eq. k) go to 70
               t = b(l)
               b(l) = b(k)
               b(k) = t
   70       continue
   80    continue
   90    continue
  100 continue
      return
      end
      subroutine daxpy(n,da,dx,incx,dy,incy)
c
c     constant times a vector plus a vector.
c     uses unrolled loops for increments equal to one.
c     jack dongarra, linpack, 3/11/78.
c
      double precision dx(1),dy(1),da
      integer i,incx,incy,ix,iy,m,mp1,n
c
      if(n.le.0)return
      if (da .eq. 0.0d0) return
      if(incx.eq.1.and.incy.eq.1)go to 20
c
c        code for unequal increments or equal increments
c          not equal to 1
c
      ix = 1
      iy = 1
      if(incx.lt.0)ix = (-n+1)*incx + 1
      if(incy.lt.0)iy = (-n+1)*incy + 1
      do 10 i = 1,n
        dy(iy) = dy(iy) + da*dx(ix)
        ix = ix + incx
        iy = iy + incy
   10 continue
      return
c
c        code for both increments equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,4)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dy(i) = dy(i) + da*dx(i)
   30 continue
      if( n .lt. 4 ) return
   40 mp1 = m + 1
      do 50 i = mp1,n,4
        dy(i) = dy(i) + da*dx(i)
        dy(i + 1) = dy(i + 1) + da*dx(i + 1)
        dy(i + 2) = dy(i + 2) + da*dx(i + 2)
        dy(i + 3) = dy(i + 3) + da*dx(i + 3)
   50 continue
      return
      end
      subroutine dscal(n,da,dx,incx)
c
c     scales a vector by a constant.
c     uses unrolled loops for increment equal to one.
c     jack dongarra, linpack, 3/11/78.
c
      double precision da,dx(1)
      integer i,incx,m,mp1,n,nincx
c
      if(n.le.0)return
      if(incx.eq.1)go to 20
c
c        code for increment not equal to 1
c
      nincx = n*incx
      do 10 i = 1,nincx,incx
        dx(i) = da*dx(i)
   10 continue
      return
c
c        code for increment equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,5)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dx(i) = da*dx(i)
   30 continue
      if( n .lt. 5 ) return
   40 mp1 = m + 1
      do 50 i = mp1,n,5
        dx(i) = da*dx(i)
        dx(i + 1) = da*dx(i + 1)
        dx(i + 2) = da*dx(i + 2)
        dx(i + 3) = da*dx(i + 3)
        dx(i + 4) = da*dx(i + 4)
   50 continue
      return
      end
      double precision function ddot(n,dx,incx,dy,incy)
c
c     forms the dot product of two vectors.
c     uses unrolled loops for increments equal to one.
c     jack dongarra, linpack, 3/11/78.
c
      double precision dx(1),dy(1),dtemp
      integer i,incx,incy,ix,iy,m,mp1,n
c
      ddot = 0.0d0
      dtemp = 0.0d0
      if(n.le.0)return
      if(incx.eq.1.and.incy.eq.1)go to 20
c
c        code for unequal increments or equal increments
c          not equal to 1
c
      ix = 1
      iy = 1
      if(incx.lt.0)ix = (-n+1)*incx + 1
      if(incy.lt.0)iy = (-n+1)*incy + 1
      do 10 i = 1,n
        dtemp = dtemp + dx(ix)*dy(iy)
        ix = ix + incx
        iy = iy + incy
   10 continue
      ddot = dtemp
      return
c
c        code for both increments equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,5)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dtemp = dtemp + dx(i)*dy(i)
   30 continue
      if( n .lt. 5 ) go to 60
   40 mp1 = m + 1
      do 50 i = mp1,n,5
        dtemp = dtemp + dx(i)*dy(i) + dx(i + 1)*dy(i + 1) +
     *   dx(i + 2)*dy(i + 2) + dx(i + 3)*dy(i + 3) + dx(i + 4)*dy(i + 4)
   50 continue
   60 ddot = dtemp
      return
      end
      integer function idamax(n,dx,incx)
c
c     finds the index of element having max. absolute value.
c     jack dongarra, linpack, 3/11/78.
c
      double precision dx(1),dmax
      integer i,incx,ix,n
c
      idamax = 0
      if( n .lt. 1 ) return
      idamax = 1
      if(n.eq.1)return
      if(incx.eq.1)go to 20
c
c        code for increment not equal to 1
c
      ix = 1
      dmax = dabs(dx(1))
      ix = ix + incx
      do 10 i = 2,n
         if(dabs(dx(ix)).le.dmax) go to 5
         idamax = i
         dmax = dabs(dx(ix))
    5    ix = ix + incx
   10 continue
      return
c
c        code for increment equal to 1
c
   20 dmax = dabs(dx(1))
      do 30 i = 2,n
         if(dabs(dx(i)).le.dmax) go to 30
         idamax = i
         dmax = dabs(dx(i))
   30 continue
      return
      end
      subroutine  dcopy(n,dx,incx,dy,incy)
c
c     copies a vector, x, to a vector, y.
c     uses unrolled loops for increments equal to one.
c     jack dongarra, linpack, 3/11/78.
c
      double precision dx(1),dy(1)
      integer i,incx,incy,ix,iy,m,mp1,n
c
      if(n.le.0)return
      if(incx.eq.1.and.incy.eq.1)go to 20
c
c        code for unequal increments or equal increments
c          not equal to 1
c
      ix = 1
      iy = 1
      if(incx.lt.0)ix = (-n+1)*incx + 1
      if(incy.lt.0)iy = (-n+1)*incy + 1
      do 10 i = 1,n
        dy(iy) = dx(ix)
        ix = ix + incx
        iy = iy + incy
   10 continue
      return
c
c        code for both increments equal to 1
c
c
c        clean-up loop
c
   20 m = mod(n,7)
      if( m .eq. 0 ) go to 40
      do 30 i = 1,m
        dy(i) = dx(i)
   30 continue
      if( n .lt. 7 ) return
   40 mp1 = m + 1
      do 50 i = mp1,n,7
        dy(i) = dx(i)
        dy(i + 1) = dx(i + 1)
        dy(i + 2) = dx(i + 2)
        dy(i + 3) = dx(i + 3)
        dy(i + 4) = dx(i + 4)
        dy(i + 5) = dx(i + 5)
        dy(i + 6) = dx(i + 6)
   50 continue
      return
      end
