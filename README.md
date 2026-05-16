# Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm

This repository contains the official implementation of the paper:  
**"Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm"** by Ichiro Kato.

## Overview / 概要 
本プロジェクトは、平面グラフの四色問題を、カタログ（静的なパターン集）に依存せず、論理的なシーケンスのみで決定論的に解決する新しいアルゴリズムを提供します。

This project introduces a novel algorithm that solves the Four-Color Map Theorem purely through logical sequences without relying on pre-defined catalogs. By utilizing **Exit Nodes** and **Three-Way Swap** sequences, it achieves deterministic coloring with an efficiency of $O(N^{1.7})$.

## Key Features / 特徴
- **Catalog-Free**: カタログを一切持たず、計算によって彩色を決定します。
- **Deterministic**: 確率的な挙動を排除し、デッドロックを論理的に回避します。
- **High Performance**: 100万件以上のテストをクリアし、大規模なグラフでも安定して動作します。
- **Highly Reproducible**: 論文の記述に基づき、極めて高い再現性を備えています。

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
