# Theseus: H_submitter Production Utilities

## ro-model
Usage:
	`ro-model [-v] [-J] [-B] [-S] [-K] [-W] [-g gaussian-prop] <sigma>` <br />
* Produce a min entropy estimate using the selected stochastic model.
* Sigma is the observed normalized (period length) jitter standard deviation (expressed as a proportion of the ring oscillator period).
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-J`: Use the Jackson stochastic model.
* `-B`: Use the BLMT stochastic model.
* `-S`: Use the Saarinen stochastic model.
* `-K`: Use the Killmann-Schindler stochastic model.
* `-W`: Use the worst-case Killmann-Schindler stochastic model.
* `-g <r>`: Operate under the assumption that the unpredictable portion of the observed jitter is sigma*r (so reduce the observed jitter by the factor r).

Some example results are contained within the following graphs: <br />
![Ring Oscillator Stochastic Model Results](../docs/images/ro-model.png)

![Zoomed In Ring Oscillator Stochastic Model Results](../docs/images/ro-model-zoom.png)

Note that the Killmann-Schindler stochastic model and the Saarinen stochastic model produce effectively equivalent results.

## linear-interpolate
Usage:
	`linear-interpolate [-v] [-i] [-x] <value>`
* Takes a set of points (`x_1`, `y_1`), ... (`x_n`, `y_n`), one point per line, from stdin and treats the points as a relation.
* The relation is then forced to be functional by discarding point; by default, the point with the lowest value (y-value) is retained for each distinct argument (x-value), unless otherwise directed.
* The points in the resulting function are then used to define and extension function f: `[x_1, x_n] -> R`, where the values are established through linear interpolation.
* The reference data may include the arguments (x-values) INFINITY (or INF) and -INFINITY (or -INF) as end arguments (`x_1` or `x_n`). These are a flag that the function should be extended as a constant function.
* The value of such points must be equal to the nearest argument (x-value).
* `-v`:      Verbose mode (can be used up to 3 times for increased verbosity).
* `-i`:      After the described relation is turned into a function, the relation's coordinates are exchanged, and points are discarded from the resulting relation until it is again a function (this is in some sense an inverse function).
*`-x`:      If we encounter a relation with multiple equal arguments, keep the one with the largest value (y-value).

This tool can be used to infer parameters from the statistical results.