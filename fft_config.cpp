// fft_config.cpp
// Header‑only constants — nothing to implement here.

/* NOTES
 *
 * implement 2 freq domain mode, one will only go to 100 khz,the other is what we have rn
 * this one will then have a sample rate of 2X freq, and hence we need another thread for this
 * This maybe change size FFT buffer
 * Drop down menu to toggle between modes
 *
 *  ///// strategy:
 *
 *  First off, modularize, implement new class and file, which handles all plotting systems
 *
 *  we must still change the following:
 *  FFT size - make it adjustable per mode
 *
 *  ploting (axis) / change to khz when mode switch
 *  buffer sizes
 *
 *
*/

/* Improvements
 *  FIX adjustable time, no more bugs. This first
 * full screenable, not too hard
 * saving feature, save the plot, (maybe rerun it too?)
 * currently unable to implement .mat download, big drag, buggy, if needed worry abt later but csv should be fine
 * panning and zoom in/out on graphs
 * light mode/ dark mode
 * performance statistics
 * any edge cases?
 *
 *
 */

/* Testing
 *
 * uhhh
*/
