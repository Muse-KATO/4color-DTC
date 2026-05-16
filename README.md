# Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm

This repository contains the official implementation of the paper:  
**"Dynamic Topology Control: A Catalog-Free and Deterministic $O(N^{1.7})$ Four-Coloring Algorithm"** by Ichiro Kato.

## Overview / 概要 
本プロジェクトは、平面グラフの四色問題を、カタログ（静的なパターン集）に依存せず、論理的なシーケンスのみで決定論的に解決する新しいアルゴリズムを提供します。

This project introduces a novel algorithm that solves the Four-Color Map Theorem purely through logical sequences without relying on pre-defined catalogs. By utilizing **Exit Nodes** and **Three-Way Swap** sequences, it achieves deterministic coloring with an efficiency of $O(N^{1.7})$.

### Key Features / 主な特徴

- **Catalog-Free (カタログフリー / 構成カタログの排除)**
  - Decides coloring entirely through topological computation, eliminating the need for massive traditional reducible configuration catalogs.
  - 従来の膨大な可約構成カタログを一切排除し、純粋なトポロジー計算のみによって彩色を決定します。

- **Deterministic & Logically Proven (決定論的アプローチ / デッドロックの論理的回避)**
  - Eradicates probabilistic behaviors and ensures an $O(N^{1.7})$ upper bound, resolving coloring deadlocks logically via Exit Nodes and Three-Way Swaps.
  - 確率的な挙動を完全に排除して $O(N^{1.7})$ の計算量を保証し、Exit NodeやThree-Way Swapを用いて彩色デッドロックを論理的に回避します。

- **High-Performance Verification (高パフォーマンス / 100万件超の実証実績)**
  - Proven stable and robust through automated batch testing of over 1,002,000 massive, complex planar maps without a single deadlock.
  - 100万件＋2000件の大規模かつ複雑な平面グラフによる自動バッチテストを完全撃破し、デッドロックゼロの圧倒的な安定性を実証済みです。

- **Highly Reproducible (極めて高い再現性 / オープンソース実証)**
  - Provides fully reproducible C source code and verification software (`4Cols.exe`) constructed precisely upon the manuscript's formulations.
  - 論文の記述に完全に基づき構築されたC言語ソースコードと検証ソフトを公開しており、誰でも極めて高い再現性で検証可能です。

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
