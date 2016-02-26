/*
 * Copyright (c) 2013-2015, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define FLASH_ORIGIN 0x08000000
#define FLASH_LENGTH 0x200000

/* 
 * 0x10000000 is the start address of CCM SRAM,
 * 0x20000000 is the start address of normal SRAM.
 */
#define SRAM_ORIGIN  0x10000000
#define SRAM_LENGTH  0x10000