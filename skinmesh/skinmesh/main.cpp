// 極短 XFile Skin Mesh Animation！

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include <windows.h>
#include <tchar.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <map>
#include <string>
#include "main.h"


TCHAR gName[] = _T("XFile Skin Mesh Animation");


SM::SMD3DXMESHCONTAINER *getMeshContainer(D3DXFRAME *frame) {
	if (frame->pMeshContainer)
		return (SM::SMD3DXMESHCONTAINER*)frame->pMeshContainer;
	if (frame->pFrameFirstChild)
		return getMeshContainer(frame->pFrameFirstChild);
	if (frame->pFrameSibling)
		return getMeshContainer(frame->pFrameSibling);
	return 0;
}

void setFrameId(SM::SMD3DXFRAME *frame, ID3DXSkinInfo *info) {
	std::map<std::string, DWORD> nameToIdMap;
	for (DWORD i = 0; i < info->GetNumBones(); i++)
		nameToIdMap[info->GetBoneName(i)] = i;

	struct create {
		static void f(std::map<std::string, DWORD> nameToIdMap, ID3DXSkinInfo *info, SM::SMD3DXFRAME* frame) {
			if (nameToIdMap.find(frame->Name) != nameToIdMap.end()) {
				frame->id = nameToIdMap[frame->Name];
				frame->offsetMatrix = *info->GetBoneOffsetMatrix(frame->id);
			}
			if (frame->pFrameFirstChild)
				f(nameToIdMap, info, (SM::SMD3DXFRAME*)frame->pFrameFirstChild);
			if (frame->pFrameSibling)
				f(nameToIdMap, info, (SM::SMD3DXFRAME*)frame->pFrameSibling);
		}
	};
	create::f(nameToIdMap, info, frame);
}

void updateCombMatrix(std::map<DWORD, D3DXMATRIX> &combMatrixMap, SM::SMD3DXFRAME *frame) {
	struct update {
		static void f(std::map<DWORD, D3DXMATRIX> &combMatrixMap, D3DXMATRIX &parentBoneMatrix, SM::SMD3DXFRAME *frame) {
			D3DXMATRIX &localBoneMatrix = frame->TransformationMatrix;
			D3DXMATRIX boneMatrix = localBoneMatrix * parentBoneMatrix;
			if (frame->id != 0xffffffff)
				combMatrixMap[frame->id] = frame->offsetMatrix * boneMatrix;
			if (frame->pFrameFirstChild)
				f(combMatrixMap, boneMatrix, (SM::SMD3DXFRAME*)frame->pFrameFirstChild);
			if (frame->pFrameSibling)
				f(combMatrixMap, parentBoneMatrix, (SM::SMD3DXFRAME*)frame->pFrameSibling);
		}
	};
	D3DXMATRIX iden;
	D3DXMatrixIdentity(&iden);
	update::f(combMatrixMap, iden, frame);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT mes, WPARAM wParam, LPARAM lParam) {
	if (mes == WM_DESTROY) { PostQuitMessage(0); return 0; }
	return DefWindowProc(hWnd, mes, wParam, lParam);
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	// アプリケーションの初期化
	MSG msg; HWND hWnd;
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, (TCHAR*)gName, NULL };
	if (!RegisterClassEx(&wcex))
		return 0;

	int sw = 600, sh = 600;
	RECT r = { 0, 0, sw, sh };
	::AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
	if (!(hWnd = CreateWindow(gName, gName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, r.right - r.left, r.bottom - r.top, NULL, NULL, hInstance, NULL)))
		return 0;

	// Direct3Dの初期化
	LPDIRECT3D9 g_pD3D;
	LPDIRECT3DDEVICE9 g_pD3DDev;
	if (!(g_pD3D = Direct3DCreate9(D3D_SDK_VERSION))) return 0;

	D3DPRESENT_PARAMETERS d3dpp = { sw , sh, D3DFMT_X8R8G8B8, 0, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD, NULL, TRUE, TRUE, D3DFMT_D24S8, 0, 0, D3DPRESENT_INTERVAL_DEFAULT };

	if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pD3DDev))) {
		g_pD3D->Release();
		return 0;
	}

	// スキンメッシュ情報をXファイルから取得
	SM::AllocateHierarchy allocater;
	SM::SMD3DXFRAME *pRootFrame = 0;
	ID3DXAnimationController *controller = 0;
	D3DXLoadMeshHierarchyFromX(_T("resource/tiny.x"), D3DXMESH_MANAGED, g_pD3DDev, &allocater, 0, (D3DXFRAME**)&pRootFrame, &controller);

	SM::SMD3DXMESHCONTAINER *cont = getMeshContainer(pRootFrame);
	D3DXBONECOMBINATION *combs = (D3DXBONECOMBINATION*)cont->boneCombinationTable->GetBufferPointer();

	// フレーム内にボーンIDとオフセット行列を埋め込む
	setFrameId(pRootFrame, cont->pSkinInfo);

	// テクスチャ作成
	IDirect3DTexture9 *tex = 0;
	D3DXCreateTextureFromFile(g_pD3DDev, _T("resource/Tiny_skin.dds"), &tex);

	// ビュー、射影変換行列設定
	D3DXMATRIX world, view, proj;
	D3DXMatrixIdentity(&world);
	D3DXMatrixLookAtLH(&view, &D3DXVECTOR3(0.0f, 0.0f, -1500.0f), &D3DXVECTOR3(0.0f, 0.0f, 0.0f), &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
	D3DXMatrixPerspectiveFovLH(&proj, D3DXToRadian(30.0f), 1.0f, 0.01f, 10000.0f);
	g_pD3DDev->SetTransform(D3DTS_VIEW, &view);
	g_pD3DDev->SetTransform(D3DTS_PROJECTION, &proj);

	// ライトオフ
	g_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);


	ShowWindow(hWnd, nCmdShow);

	// メッセージ ループ
	std::map<DWORD, D3DXMATRIX> combMatrixMap;
	do {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { DispatchMessage(&msg); }
		else {
			// 時間を進めて姿勢更新
			controller->AdvanceTime(0.016f, 0);
			updateCombMatrix(combMatrixMap, pRootFrame);

			// Direct3Dの処理
			g_pD3DDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0);
			g_pD3DDev->BeginScene();

			g_pD3DDev->SetTexture(0, tex);
			for (DWORD i = 0; i < cont->numBoneCombinations; i++) {
				DWORD boneNum = 0;
				for (DWORD j = 0; j < cont->maxFaceInfl; j++) {
					DWORD id = combs[i].BoneId[j];
					if (id != UINT_MAX) {
						g_pD3DDev->SetTransform(D3DTS_WORLDMATRIX(j), &combMatrixMap[id]);
						boneNum++;
					}
				}
				g_pD3DDev->SetRenderState(D3DRS_VERTEXBLEND, boneNum - 1);
				cont->MeshData.pMesh->DrawSubset(i);
			}

			g_pD3DDev->EndScene();
			g_pD3DDev->Present(NULL, NULL, NULL, NULL);
		}
	} while (msg.message != WM_QUIT);

	// スキンメッシュ情報削除
	allocater.DestroyFrame(pRootFrame);

	tex->Release();
	g_pD3DDev->Release();
	g_pD3D->Release();

	return 0;
}