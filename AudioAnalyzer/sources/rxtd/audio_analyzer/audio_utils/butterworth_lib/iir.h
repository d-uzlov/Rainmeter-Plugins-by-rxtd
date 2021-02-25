#pragma once

double* binomial_mult(int n, double* p);
double* trinomial_mult(int n, double* b, double* c);

double* dcof_bwlp(int n, double fcf, double unused);
double* dcof_bwhp(int n, double fcf, double unused);
double* dcof_bwbp(int n, double f1f, double f2f);
double* dcof_bwbs(int n, double f1f, double f2f);

double* ccof_bwlp(int n, double fcf, double unused);
double* ccof_bwhp(int n, double fcf, double unused);
double* ccof_bwbp(int n, double f1f, double f2f);
double* ccof_bwbs(int n, double f1f, double f2f);

double sf_bwlp(int n, double fcf, double unused);
double sf_bwhp(int n, double fcf, double unused);
double sf_bwbp(int n, double f1f, double f2f);
double sf_bwbs(int n, double f1f, double f2f);
