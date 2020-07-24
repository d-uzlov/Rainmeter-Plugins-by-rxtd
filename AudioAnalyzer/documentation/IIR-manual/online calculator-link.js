/*
    Biquad coefficient calculator

    copyright 2010-2013 Nigel Redmon
*/

jQuery(document).ready(function( $ ) {
	drawIt_biquad2();
});

function setit(sliderID, paramID, factor) {
	document.getElementById(paramID).value = document.getElementById(sliderID).value / factor;
	drawIt_biquad2();
}


function drawIt_biquad2() {
	var type = document.biquad_params.Type.value;
	var plotType = document.biquad_params.PlotType.value;

    var FsField = document.getElementById("biquad_Fs");
	var Fs = parseFloat(FsField.value);
	if (Fs < 1)
		Fs = 1;
	FsField.value = Fs;

    var FcField = document.getElementById("biquad_Fc");
	var Fc = parseFloat(FcField.value);
	if (Fc > Fs/2)
		Fc = Fs/2;
	else if (Fc < 0)
		Fc = 0;
	FcField.value = Fc;

	var QField = document.getElementById("biquad_Q");
	var Q = parseFloat(QField.value);
	if (Q < 0.01)
		Q = 0.01;
	QField.value = Q;

    var gainField = document.getElementById("biquad_Gain");
	var gain = parseFloat(gainField.value);
	gainField.value = gain;

	switch (type) {
		case "lowpass":
		case "highpass":
		case "bandpass":
		case "notch":
			QField.disabled = false;
			gainField.disabled = true;
			break;
		case "peak":
			QField.disabled = false;
			gainField.disabled = false;
			break			
		case "highShelf":
		case "lowShelf":
			QField.disabled = true;
			gainField.disabled = false;
			break			
		default:
			QField.disabled = true;
			gainField.disabled = true;
	}

	calcBiquad(type, Fc, Fs, Q, gain, plotType);
}


//
// calcBiquad
//
// Dec 14, 2010 njr
//
function calcBiquad(type, Fc, Fs, Q, peakGain, plotType) {
	var a0,a1,a2,b1,b2,norm;
	var ymin, ymax, minVal, maxVal;
	
	var V = Math.pow(10, Math.abs(peakGain) / 20);
	var K = Math.tan(Math.PI * Fc / Fs);
	switch (type) {
		case "one-pole lp":
			b1 = Math.exp(-2.0 * Math.PI * (Fc / Fs));
            a0 = 1.0 - b1;
            b1 = -b1;
			a1 = a2 = b2 = 0;
			break;
            
		case "one-pole hp":
			b1 = -Math.exp(-2.0 * Math.PI * (0.5 - Fc / Fs));
            a0 = 1.0 + b1;
            b1 = -b1;
			a1 = a2 = b2 = 0;
			break;
            
		case "lowpass":
			norm = 1 / (1 + K / Q + K * K);
			a0 = K * K * norm;
			a1 = 2 * a0;
			a2 = a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / Q + K * K) * norm;
			break;
		
		case "highpass":
			norm = 1 / (1 + K / Q + K * K);
			a0 = 1 * norm;
			a1 = -2 * a0;
			a2 = a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / Q + K * K) * norm;
			break;
		
		case "bandpass":
			norm = 1 / (1 + K / Q + K * K);
			a0 = K / Q * norm;
			a1 = 0;
			a2 = -a0;
			b1 = 2 * (K * K - 1) * norm;
			b2 = (1 - K / Q + K * K) * norm;
			break;
		
		case "notch":
			norm = 1 / (1 + K / Q + K * K);
			a0 = (1 + K * K) * norm;
			a1 = 2 * (K * K - 1) * norm;
			a2 = a0;
			b1 = a1;
			b2 = (1 - K / Q + K * K) * norm;
			break;
		
		case "peak":
			if (peakGain >= 0) {
				norm = 1 / (1 + 1/Q * K + K * K);
				a0 = (1 + V/Q * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - V/Q * K + K * K) * norm;
				b1 = a1;
				b2 = (1 - 1/Q * K + K * K) * norm;
			}
			else {	
				norm = 1 / (1 + V/Q * K + K * K);
				a0 = (1 + 1/Q * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - 1/Q * K + K * K) * norm;
				b1 = a1;
				b2 = (1 - V/Q * K + K * K) * norm;
			}
			break;
		case "lowShelf":
			if (peakGain >= 0) {
				norm = 1 / (1 + Math.SQRT2 * K + K * K);
				a0 = (1 + Math.sqrt(2*V) * K + V * K * K) * norm;
				a1 = 2 * (V * K * K - 1) * norm;
				a2 = (1 - Math.sqrt(2*V) * K + V * K * K) * norm;
				b1 = 2 * (K * K - 1) * norm;
				b2 = (1 - Math.SQRT2 * K + K * K) * norm;
			}
			else {	
				norm = 1 / (1 + Math.sqrt(2*V) * K + V * K * K);
				a0 = (1 + Math.SQRT2 * K + K * K) * norm;
				a1 = 2 * (K * K - 1) * norm;
				a2 = (1 - Math.SQRT2 * K + K * K) * norm;
				b1 = 2 * (V * K * K - 1) * norm;
				b2 = (1 - Math.sqrt(2*V) * K + V * K * K) * norm;
			}
			break;
		case "highShelf":
            if (peakGain >= 0) {
                norm = 1 / (1 + Math.SQRT2 * K + K * K);
                a0 = (V + Math.sqrt(2*V) * K + K * K) * norm;
                a1 = 2 * (K * K - V) * norm;
                a2 = (V - Math.sqrt(2*V) * K + K * K) * norm;
                b1 = 2 * (K * K - 1) * norm;
                b2 = (1 - Math.SQRT2 * K + K * K) * norm;
            }
            else {	
                norm = 1 / (V + Math.sqrt(2*V) * K + K * K);
                a0 = (1 + Math.SQRT2 * K + K * K) * norm;
                a1 = 2 * (K * K - 1) * norm;
                a2 = (1 - Math.SQRT2 * K + K * K) * norm;
                b1 = 2 * (K * K - V) * norm;
                b2 = (V - Math.sqrt(2*V) * K + K * K) * norm;
			}
			break;
	}

	var len = 512;
	var magPlot = [];
	for (var idx = 0; idx < len; idx++) {
		var w;
		if (plotType == "linear")
			w = idx / (len - 1) * Math.PI;	// 0 to pi, linear scale
		else
			w = Math.exp(Math.log(1 / 0.001) * idx / (len - 1)) * 0.001 * Math.PI;	// 0.001 to 1, times pi, log scale

		var phi = Math.pow(Math.sin(w/2), 2);
		var y = Math.log(Math.pow(a0+a1+a2, 2) - 4*(a0*a1 + 4*a0*a2 + a1*a2)*phi + 16*a0*a2*phi*phi) - Math.log(Math.pow(1+b1+b2, 2) - 4*(b1 + 4*b2 + b1*b2)*phi + 16*b2*phi*phi);
		y = y * 10 / Math.LN10
		if (y == -Infinity)
			y = -200;

		if (plotType == "linear")
			magPlot.push([idx / (len - 1) * Fs / 2, y]);
		else
			magPlot.push([idx / (len - 1) / 2, y]);

		if (idx == 0)
			minVal = maxVal = y;
		else if (y < minVal)
			minVal = y;
		else if (y > maxVal)
			maxVal = y;
	}

	// configure y-axis
	switch (type) {
		default:
		case "lowpass":
		case "highpass":
		case "bandpass":
		case "notch":
			ymin = -100;
			ymax = 0;
			if (maxVal > ymax)
				ymax = maxVal;
			break;
		case "peak":
		case "lowShelf":
		case "highShelf":
			ymin = -10;
			ymax = 10;
			if (maxVal > ymax)
				ymax = maxVal;
			else if (minVal < ymin)
				ymin = minVal;
			break;
        case "one-pole lp":
        case "one-pole hp":
			ymin = -40;
			ymax = 0;
            break;
	}
	if (plotType == "linear")
		Flotr.draw(document.getElementById('container-20131013'), [ magPlot], { yaxis: { max: ymax, min: ymin} });
	else
		Flotr.draw(document.getElementById('container-20131013'), [ magPlot], { yaxis: { max: ymax, min: ymin}, xaxis: {tickFormatter: nullTickFormatter} });

	// list coefficients
	var coefsList = "a0 = " + a0 + "\n";
	coefsList += "a1 = " + a1 + "\n";
	coefsList += "a2 = " + a2 + "\n";
	coefsList += "b1 = " + b1 + "\n";
	coefsList += "b2 = " + b2;
    var taNode = document.getElementById("biquad_coefsList");
    // remove existing child txt node
    while (taNode.firstChild)
      taNode.removeChild(taNode.firstChild);
    var listNode = document.createTextNode(coefsList);
    taNode.appendChild(listNode);
}

function nullTickFormatter(xval) {
    	var FsField = document.getElementById("biquad_Fs");
		var Fs = parseFloat(FsField.value);

		var val = Math.exp(Math.log(1 / 0.001) * xval * 2) * 0.001 * Fs * .5;
		if (val < 1)
			return val.toFixed(3);
		if (val < 10)
			return val.toFixed(2);
		return val.toFixed(1);
}
