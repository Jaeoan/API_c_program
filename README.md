# 🏋️‍♂️ C-Based On-Device AI Health Planner

> **임베디드 환경을 위한 C언어 기반 생성형 AI(Gemini 2.5) 연동 헬스케어 시스템**

[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![API](https://img.shields.io/badge/AI-Google%20Gemini%202.5-orange)](https://deepmind.google/technologies/gemini/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Embedded(Planned)-lightgrey)]()

이 프로젝트는 고수준 언어(Python 등)의 라이브러리에 의존하지 않고, **C언어(Low-level)** 환경에서 직접 메모리와 HTTP 프로토콜을 제어하여 **Google Gemini API**와 통신하는 헬스 플래너 어플리케이션입니다.

향후 **ARM Cortex-M 기반 MCU(독립형 하드웨어)**에 이식하여, PC 없이 동작하는 키오스크형 헬스케어 디바이스를 만들기 위한 선행 연구 프로젝트입니다.

---

---

## 📝 프로젝트 개요

* **개발 목표:** 리소스가 제한된 임베디드/엣지 디바이스 환경에서 LLM(거대언어모델)을 제어하는 온디바이스 AI 기술 구현.
* **사용 언어:** Standard C (C99)
* **개발 환경:** Visual Studio 2022 (Windows)
* **API 모델:** Google Gemini 2.5 Flash

## ✨ 주요 기능

* **📋 사용자 정보 데이터화:** 이름, 신체 정보(키, 체중, 골격근량), 알레르기 등을 구조체로 수집.
* **🤖 동적 프롬프트 엔지니어링:** 입력 데이터를 바탕으로 AI 트레이너 페르소나에게 보낼 정교한 프롬프트 조립.
* **🔣 이중 인코딩 파이프라인 (Transcoding):** Windows(CP949)와 AI(UTF-8) 간의 문자열 깨짐 현상을 해결하기 위한 실시간 변환 구현.
* **💾 가변 메모리 버퍼링:** AI의 긴 답변(Streaming Data)을 처리하기 위해 Callback 함수와 `realloc`을 활용한 동적 메모리 관리.
* **file 결과 저장:** 식단/운동 루틴을 텍스트(.txt) 및 JSON(.json) 파일로 저장 (BOM 처리 포함).

---

## 🏗 시스템 아키텍처

데이터의 흐름에 따라 **인코딩 변환(CP949 ↔ UTF-8)**이 어떻게 이루어지는지가 핵심입니다.

```mermaid
graph LR
    User[사용자 입력] -->|CP949| Encode[인코딩 변환]
    Encode -->|UTF-8| Packing[JSON 패킹]
    Packing -->|HTTPS POST| API[Gemini API]
    API -->|Inference| AI{AI 추론}
    AI -->|Response| Parsing[JSON 파싱]
    Parsing -->|Decoding| Console[화면 출력 (CP949)]
    Parsing -->|BOM| File[파일 저장 (UTF-8)]
