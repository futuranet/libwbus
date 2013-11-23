#!/usr/bin/octave -q

# Something resembling the air compressor control loop

# Set point in mBar
sp = horzcat(zeros(1,500), ones(1,500)*300, ones(1,500)*500, zeros(1,500), ones(1,500)*200, ones(1,500)*700, zeros(1,500));

N = size(sp)(2);

# compressor motor threshold to start delivering compressed air

global CCVTHRES = 4;
global CCMINP = 200;
global CCK = 0.1;
global CC_DELAYLINE = 10;
global ccout = zeros(CC_DELAYLINE);
global out = 0;
global ccout_i = 1;

function result = motor(v)
	global CCVTHRES;
	global CCMINP;
	global CCK;
	global CC_DELAYLINE;
	global ccout;
	global ccout_i;
	global out;

	target = 0;
	if (v > CCVTHRES)
		target = CCMINP + v*(1000/12);
	endif
	out = (target-out)*CCK;
	ccout(ccout_i) = out;

	ccout_i = mod((ccout_i + 1), CC_DELAYLINE-1) + 1;

	result = ccout(ccout_i);

endfunction

# Integer scale
CC_SCALE=fixed(16,0,256);

# PID coefficients
CC_P=fixed(16, 0, 8);
CC_I=fixed(16, 0, 3);
CC_D=fixed(16, 0, 1);

# last error for derivative calculation
x1 = fixed(16, 0, 0);
# Integral
cc_i = fixed(16, 0, 0);
# Output value in Volt
val = fixed(16, 0, 0);

for t=(1:N)
  m = motor(float(val));
  err = fixed(30, 0, sp(t)-m);

  cc_i += err;
  cc_d = fixed(16, 0, err-x1);

  val = err*CC_P + cc_d*CC_D + cc_i*CC_I;
  val = fixed(16, 0, val / CC_SCALE);

  x1 = err;

  Y(t) = val;
  E(t) = err;
  M(t) = m;
endfor

# Plot results
hold on
plot(float(Y), "b;controller output;")
plot(float(E), "r;error;")
plot(float(M), "g;modelled plant output / motor RPS;")
pause
