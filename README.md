# Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm

This repository contains the official implementation of the paper:  
**"Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm"** by Ichiro Kato.

## Overview / 概要 

This project introduces a novel algorithm that solves the Four-Color Map Theorem purely through logical sequences without relying on pre-defined catalogs. By utilizing **Exit Nodes** and **Three-Way Swap** sequences, it achieves deterministic coloring with an efficiency of $O(N^{1.7})$.

本プロジェクトは平面グラフの四色問題を、従来の可約構成カタログ（静的なパターン集）に一切依存せず、論理的なシーケンスのみで決定論的に解決する、新しいアルゴリズムを提供します。「抜穴ノード」や「三つ巴スワップ」といった動的トポロジーシーケンスを活用することで、最悪計算量 $O(N^{1.7})$ という高い処理効率での彩色を実現しています。

## Key Features / 主な特徴

* **Catalog-Free (カタログフリー)**
  * Eliminates reducible catalogs; decides coloring purely via dynamic computation.
  * 可約構成カタログを一切排除し、純粋な動的トポロジー計算のみで彩色。

* **Deterministic $O(N^{1.7})$ (決定論的・最悪計算量の保証)**
  * Eradicates probabilistic bypasses; solves deadlocks via Exit Nodes & Three-Way Swaps.
  * 確率的挙動をゼロ化。Exit NodeとThree-Way Swapでデッドロックを論理回避。

* **1M+ Maps Verified (100万件超の検証実績)**
  * Passed strict automated batch testing of over 1,000,000 complex planar maps.
  * 100万件＋2000件の複雑な巨大平面グラフによる自動バッチテストを完全撃破。

* **100% Reproducible (高い再現性・オープンソース化)**
  * Provides complete C source code and verification software (`4Cols.exe`).
  * 論文に完全準拠したC言語ソースコードと検証ソフトをすべて公開。

## Verification Software / 検証ソフトウェア
The provided C source code allows you to:
1. Generate complex planar maps (Map Generator).
2. Execute the Dynamic Topology Control coloring engine.
3. Verify the correctness of the result against the Four Color Theorem.

## Getting Started / 使い方
cl /c /W3 /O2 /Oi /GS- /GL /Gy /Gw 4Cols.c  
link /FIXED /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /NOCOFFGRPINFO /LARGEADDRESSAWARE 4Cols.obj

## Documentation / 論文
For a detailed theoretical background, please refer to the paper:
- [Link to PDF / arXiv link - *Coming Soon*]

---
*Created by Ichiro Kato (Muse-KATO). After 6 months of intense logical development, this sequence was perfected.*
