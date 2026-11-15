# Metal Slug GBA — 원본 파일 기반 로컬 빌더

Bangoo의 비공식 GBA 팬 포트입니다. 사용자가 준비한 원본 `mslug.zip`과 Neo Geo BIOS `neogeo.zip`에서 필요한 데이터를 추출·변환하고, 편집한 그래픽 변경분을 적용하여 GBA ROM을 만듭니다.

원본 ROM·BIOS·완성된 원작 그래픽·음원·GBA ROM은 이 저장소에 포함하지 않습니다. 과거 개발 이력이 없는 별도 공개 저장소이며, 복원된 파일은 사용자 컴퓨터에서만 생성됩니다.

## 빌드

Linux, Python 3.12, Git, 호스트 GCC/G++, GNU Arm Embedded GCC 14.2.Rel1 환경에서 검증했습니다.

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 build_rom.py --rom /경로/mslug.zip --bios /경로/neogeo.zip --cc /경로/arm-none-eabi-gcc
```

원본 ZIP 내부 파일의 이름·크기·SHA-256은 [입력 목록](assets/input-hashes.json)을 확인하세요. 저작권 자료의 다운로드 링크는 제공하지 않습니다. 최초 실행 시 고정된 버전의 upstream 소스를 내려받습니다. 수 GB의 여유 공간이 필요합니다.

결과는 `dist/metal-slug-gba-native-v0.5.159-parallax-source-candidate.gba`에 생성됩니다. 편집 그래픽을 복원한 빌드 결과는 기존 v191 체크포인트와 바이트 단위로 동일합니다. 용량은 33,518,376바이트이며 SHA-256은 `635a23adea8e08650393c515080410e890b8d3103bd297f4eb587de03dd1d4d9`입니다. 이번 작업에서 별도의 실기 플레이 검증을 수행한 것은 아닙니다.

## 권리 표기

원작 게임과 캐릭터·그래픽·음악의 권리는 SNK 등을 포함한 해당 권리자에게 있습니다. GBA는 Nintendo의 상표이며 본 프로젝트는 비공식입니다. 직접 만든 작업과 원작 기반 수정 작업을 구별하며, 원본 파일을 요구하는 방식 자체가 모든 저작권 문제의 해소를 보장하지는 않습니다.

원작 데이터를 포함하지 않는 배포 구조로 변경했으며, 수정한 캐릭터·HUD·폰트는 원본 추출 결과에 변경 픽셀을 적용하여 보존합니다. 원작 비교 이미지와 완성된 편집 파일도 로컬에서만 복원합니다. [권리 고지](NOTICE.md), [제3자 고지](THIRD_PARTY_NOTICES.md), [자산 구성](ASSET_PROVENANCE.md), [상세 빌드 안내](REBUILD.md)를 참고하세요.

생성된 `art/`, `generated/`, `build/`, `dist/`와 ROM·BIOS를 공개 저장소·이슈·릴리스에 업로드하지 마세요.
