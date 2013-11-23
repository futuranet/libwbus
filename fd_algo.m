#!/usr/bin/octave -q

arguments=argv();

# Use real data from Data Top log, converted to utf8
# using something like this: iconv -f unicode -t utf8 input.data > utf8.data 
fd = dlmread(arguments{1}, SEP='\t', R0=1, C0=0);
vinput = fixed(16, 0, interp(fd(:,13),5)*1000);

# Coefficients and threshold for detector algorithm
K1=fixed(16, 0, 5);
K2=fixed(16, 0, 350);
K3=fixed(16, 0, 0);
thm = fixed(16, 0, 3200);
thl = fixed(16, 0, 3000);
thh = fixed(16, 0, 3400);

# Calculate flame detector flag

x1=fixed(16, 0, vinput(1));
x2=fixed(16, 0, vinput(1));
x=fixed(16, 0, vinput(1));

function y = median5(x)
	persistent data = fixed(16,0, zeros(1,5));

	data(1:4) = data(2:5);
	data(5)=x;

	work=data;
	for a=(1:4)
		for b=(1:4)
			if (work(b) < work(b+1))
				tmp=work(b);
				work(b)=work(b+1);
				work(b+1)=tmp;
			endif
		endfor
	endfor

	y = work(3);
endfunction

function y = median3_(x)
	if ( (x(1) > x(2)) && (x(1) > x(3)) )
		if ( x(2) > x(3) )
			y = x(2);
		else
			y = x(3);
		endif
	else
		if ( (x(2) > x(1)) && (x(2) > x(3)) )
			if ( x(1) > x(3) )
				y = x(1);
			else
				y = x(3);
			endif
		else
			if ( x(1) > x(2) )
				y = x(1);
			else
				y = x(2);
			endif
		endif
	endif
endfunction

function y = median3(x)
	persistent data = fixed(16,0, zeros(1,3));
        data(1:2) = data(2:3);
        data(3)=x;

	y = median3_(data);
endfunction

function y = median9(x)
	persistent data = fixed(16,0, zeros(1,9));
	data(1:8) = data(2:9);
        data(9)=x;

	work(1)=median3_(data(1:3));
	work(2)=median3_(data(4:6));
	work(3)=median3_(data(7:9));
	y = median3_(work);
endfunction


for t=(1:length(vinput))
	# Averaging filter.
	xr = vinput(t);

	# xr = median9(xr);
	xr = (x*fixed(3) + xr)/fixed(4);

	xf(t) = xr;
	x = xr;

	ax1=(x1-x2);
	ax2=(x-x1);
	th = thm;
	if (ax1 < fixed(16, 0, -10))
		th = thl;
	endif
	if (ax1 > fixed(16, 0, 10))
		th = thh;
	endif
	m(t) = x*K1 + ax2*K2 + (ax2-ax1)*K3;
	#m(t) = ax1;

	m2(t)=median5(m(t));

	if (m2(t) < th)
		fd_out(t) = 0;
	else
		fd_out(t) = 1;
	endif

	x2=x1;
	x1=x;
endfor

# Plot results
hold on
#axis([0,600,-1500,3500], "autox");
grid("on");
plot(float(vinput), "r;ADC Seebeck voltage;");
plot(float(xf), "m;filtered ADC signal;");
plot(float(m), "b;adjusted Seebeck voltage observer;");
plot(float(m2), "c;adjusted Seebeck voltage observer 2;");
plot(float(fd_out*2000), "g;Flame detector output;");
pause
#print -dpng fd_algo.png 
