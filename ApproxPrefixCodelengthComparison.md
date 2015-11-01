## Shannon-Fano, Huffman, Polar, and Fyffe Showdown ##

I added support for [Shannon-Fano](http://en.wikipedia.org/wiki/Shannon-Fano_coding) codes, along with several variants of [Fyffe's](http://code.google.com/p/lzham/wiki/FyffeCodes) approximate codelength generation method to LZHAM. Here are the resulting compression ratios using the latest non-released alpha version of the compressor (alpha5) on enwik9 for each approach:

```
File: enwik9, Original Size 1,000,000,000

Method							Comp. Size  	Ratio (higher is better)
Polar: 						 	203,051,146     79.6948854
Fyffe1: 					 	203,051,146     79.6948854
Fyffe3: 	 					202,947,210 	79.7052790
Fyffe2: 						202,944,046 	79.7055954
Shannon-Fano: 				 	        202,223,363 	79.7776637
Huffman: 					 	202,012,593 	79.7987407
```

Shannon-Fano is a mostly straightforward, by the books recursive implementation of the well known technique.

Fyffe1-3 are all fast fixed-point implementations of Fyffe's approach, but they differ on how they compute symbol "unhappiness":
> Fyffe1 directly compares the symbol's original frequency vs. the symbol's current effective frequency (based off the current codelength vs. the desired total pow2 frequency) to decide if the symbol's codelength should be promoted to the next smallest size. Fyffe1 turns out to be very similar to Polar's method, except Fyffe1 directly adjusts codelengths, while Polar's method directly adjusts frequencies.

> Fyffe2 uses FP math to compare the symbol's original probability vs. the effective probability. This requires an FP multiply/compare per symbol.

> Fyffe3 is like Fyffe2, except it uses fixed-point math to compare and compute probabilities. This requires an integer mul per symbol. The scale was arbitrarily set to 64.

I did not record timings in this test, just ratios. From a code complexity standpoint, [Polar's](http://www.ezcodesample.com/prefixer/prefixer_article.html) method is probably the simplest/most direct approach.

Another approach that resembles the Polar/Fyffe methods is described here (Algorithm1):
[Reducing the Length of Shannon-Fano Codes and Shannon-Fano-Elias Codes](http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBIQFjAA&url=http%3A%2F%2Fciteseerx.ist.psu.edu%2Fviewdoc%2Fdownload%3Fdoi%3D10.1.1.124.4466%26rep%3Drep1%26type%3Dpdf&ei=_4SWTNq-KYzSsAPJ7vzkCQ&usg=AFQjCNHqEAQlLiZsD3j12clMjMT_cLoqTw&sig2=_m48LX6xhLfbCMPDthIRJA)