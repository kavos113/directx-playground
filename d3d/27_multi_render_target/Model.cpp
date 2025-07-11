#include "Model.h"

#include <DirectXTex.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <iostream>
#include <map>

#define AlignCBuffer(x) (((x) + 0xff) & ~0xff)

Model::Model(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap,
    RECT rc
) : m_device(std::move(device)),
    m_descHeap(std::move(descHeap))
{
    createCopyCommands();

    loadModel(MODEL_PATH);
    createVertexBuffer();
    createIndexBuffer();

    createMatrixBuffer(rc);
    createLightBuffer();

    loadTexture(TEXTURE_PATH);

    executeCopy();
}

void Model::cleanup()
{
    CloseHandle(m_copyFenceEvent);
    m_copyFence.Reset();

    m_copyCommandList.Reset();

    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_texture.Reset();
    m_matrixBuffer.Reset();
    m_lightBuffer.Reset();

    m_copyCommandQueue.Reset();
    m_copyCommandAllocator.Reset();
}

void Model::render(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &commandList)
{
    m_angle += 0.01f;
    DirectX::XMMATRIX world = DirectX::XMMatrixRotationY(m_angle);
    m_matrixBufferData->world = world;

    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

    commandList->DrawIndexedInstanced(m_indices.size(), 1, 0, 0, 0);
}

void Model::executeBarrier(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList) const
{
    commandList->ResourceBarrier(
        m_barriers.size(),
        m_barriers.data()
    );
}

void Model::createCopyCommands()
{
    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COPY,
        IID_PPV_ARGS(&m_copyCommandAllocator)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create copy command allocator." << std::endl;
        return;
    }

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        m_copyCommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_copyCommandList)
    );
    if     (FAILED(hr))
    {
        std::cerr << "Failed to create copy command list." << std::endl;
        return;
    }

    D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_COPY,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };
    hr = m_device->CreateCommandQueue(
        &copyQueueDesc,
        IID_PPV_ARGS(&m_copyCommandQueue)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create copy command queue." << std::endl;
        return;
    }

    m_copyFenceValue = 0;
    m_copyFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_copyFenceEvent)
    {
        std::cerr << "Failed to create copy fence event." << std::endl;
        return;
    }

    hr = m_device->CreateFence(
        m_copyFenceValue,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_copyFence)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create copy fence." << std::endl;
        return;
    }
}

void Model::loadModel(const std::string &path)
{
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = ".";

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, readerConfig))
    {
        if (!reader.Error().empty())
        {
            std::cerr << "Failed to load model: " << reader.Error() << std::endl;
        }
        return;
    }

    if (!reader.Warning().empty())
    {
        std::cout << "Model load warning: " << reader.Warning() << std::endl;
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();

    std::map<Vertex, unsigned short> uniqueVertices;

    for (const auto & shape : shapes)
    {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            size_t fv = shape.mesh.num_face_vertices[f];
            for (size_t v = 0; v < fv; ++v)
            {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                Vertex vertex = {};
                vertex.position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };

                if (idx.normal_index >= 0)
                {
                    vertex.normal = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    };
                }

                if (idx.texcoord_index >= 0)
                {
                    vertex.uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                }

                if (!uniqueVertices.contains(vertex))
                {
                    uniqueVertices[vertex] = static_cast<unsigned short>(m_vertices.size());
                    m_vertices.push_back(vertex);
                }

                m_indices.push_back(uniqueVertices[vertex]);
            }
            indexOffset += fv;
        }
    }
}

void Model::createVertexBuffer()
{
    createBuffer(
        sizeof(Vertex) * m_vertices.size(),
        &m_vertexBuffer,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COMMON
    );

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
    createBuffer(
        sizeof(Vertex) * m_vertices.size(),
        &stagingBuffer,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    Vertex *vertexMap = nullptr;
    HRESULT hr = stagingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map vertex buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_vertices, vertexMap);
    stagingBuffer->Unmap(0, nullptr);

    copyBuffer(stagingBuffer, m_vertexBuffer);

    m_vertexBufferView = {
        .BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(sizeof(Vertex) * m_vertices.size()),
        .StrideInBytes = sizeof(Vertex)
    };

    barrier(
        m_vertexBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

    m_waitForCopyResources.push_back(stagingBuffer);
}

void Model::createIndexBuffer()
{
    createBuffer(
        sizeof(unsigned short) * m_indices.size(),
        &m_indexBuffer,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COMMON
    );

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
    createBuffer(
        sizeof(unsigned short) * m_indices.size(),
        &stagingBuffer,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    unsigned short *indexMap = nullptr;
    HRESULT hr = stagingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map index buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_indices, indexMap);
    stagingBuffer->Unmap(0, nullptr);

    copyBuffer(stagingBuffer, m_indexBuffer);

    m_indexBufferView = {
        .BufferLocation = m_indexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(sizeof(unsigned short) * m_indices.size()),
        .Format = DXGI_FORMAT_R16_UINT
    };

    barrier(
        m_indexBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER
    );

    m_waitForCopyResources.push_back(stagingBuffer);
}

void Model::createMatrixBuffer(RECT rc)
{
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        { 0.0f, 0.2f, -0.5f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f }
    );
    DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XM_PIDIV2,
        static_cast<float>(rc.right - rc.left) / static_cast<float>(rc.bottom - rc.top),
        0.1f,
        100.0f
    );

    createBuffer(
        AlignCBuffer(sizeof(MatrixBuffer)),
        &m_matrixBuffer,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    HRESULT hr = m_matrixBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_matrixBufferData));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map matrix buffer." << std::endl;
        return;
    }

    m_matrixBufferData->world = world;
    m_matrixBufferData->view = view;
    m_matrixBufferData->projection = projection;

    D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
        .BufferLocation = m_matrixBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = AlignCBuffer(sizeof(MatrixBuffer))
    };

    m_device->CreateConstantBufferView(
        &cbvDesc,
        cbvHandle
    );
}

void Model::createLightBuffer()
{
    DirectX::XMFLOAT3 direction{-1.0f, -3.0f, 1.0f};
    DirectX::XMFLOAT3 color{0.0f, 1.0f, 0.0f};

    createBuffer(
        AlignCBuffer(sizeof(LightBuffer)),
        &m_lightBuffer,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COMMON
    );

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
    createBuffer(
        AlignCBuffer(sizeof(LightBuffer)),
        &stagingBuffer,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    LightBuffer *map = nullptr;
    HRESULT hr = stagingBuffer->Map(0, nullptr, reinterpret_cast<void**>(&map));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map light buffer." << std::endl;
        return;
    }
    map->direction = direction;
    map->color = color;
    stagingBuffer->Unmap(0, nullptr);

    copyBuffer(stagingBuffer, m_lightBuffer);

    m_waitForCopyResources.push_back(stagingBuffer);

    barrier(
        m_lightBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

    D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();
    cbvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
        .BufferLocation = m_lightBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = AlignCBuffer(sizeof(LightBuffer))
    };
    m_device->CreateConstantBufferView(
        &cbvDesc,
        cbvHandle
    );
}

void Model::loadTexture(const std::wstring &path)
{
    DirectX::TexMetadata metadata{};
    DirectX::ScratchImage scratchImage;

    HRESULT hr = DirectX::LoadFromWICFile(
        path.c_str(),
        DirectX::WIC_FLAGS_NONE,
        &metadata,
        scratchImage
    );
    if (FAILED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            std::wcerr << L"Texture file not found: " << path << std::endl;
        }
        else
        {
            std::wcerr << L"Failed to load texture from file: " << path << L" with error: " << std::hex << hr << std::endl;
        }
        return;
    }

    const DirectX::Image *image = scratchImage.GetImage(0, 0, 0);

    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = metadata.width,
        .Height = static_cast<UINT>(metadata.height),
        .DepthOrArraySize = static_cast<UINT16>(metadata.arraySize),
        .MipLevels = static_cast<UINT16>(metadata.mipLevels),
        .Format = metadata.format,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create texture resource." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingResource;
    createBuffer(
        AlignCBuffer(image->rowPitch) * image->height,
        &stagingResource,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    uint8_t *mappedData = nullptr;
    hr = stagingResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map staging resource for texture upload." << std::endl;
        return;
    }

    for (UINT y = 0; y < metadata.height; ++y)
    {
        std::memcpy(
            mappedData + y * image->rowPitch,
            image->pixels + y * image->rowPitch,
            image->rowPitch
        );
    }
    stagingResource->Unmap(0, nullptr);

    copyTexture(stagingResource, m_texture);

    barrier(
        m_texture,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    m_waitForCopyResources.push_back(stagingResource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .Format = metadata.format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = static_cast<UINT>(metadata.mipLevels),
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        }
    };

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandle.ptr += 2 * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_device->CreateShaderResourceView(
        m_texture.Get(),
        &srvDesc,
        srvHandle
    );
}

// unsupported D3D12_HEAP_TYPE_CUSTOM
// create simple buffer(not texture)
void Model::createBuffer(
    UINT64 size,
    ID3D12Resource **buffer,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState
)
{
    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = heapType,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };

    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // Dimension = Buffer must be LAYOUT_ROW_MAJOR
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(buffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create buffer resource." << std::endl;
        return;
    }
}

void Model::copyTexture(
    const Microsoft::WRL::ComPtr<ID3D12Resource> &srcBuffer,
    const Microsoft::WRL::ComPtr<ID3D12Resource> &dstBuffer
) const
{
    D3D12_RESOURCE_DESC resourceDesc = dstBuffer->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT64 requiredSize = 0;
    m_device->GetCopyableFootprints(
        &resourceDesc,
        0,
        1,
        0,
        &layout,
        nullptr,
        nullptr,
        &requiredSize
    );

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {
        .pResource = srcBuffer.Get(),
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = layout
    };

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {
        .pResource = dstBuffer.Get(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0
    };

    m_copyCommandList->CopyTextureRegion(
        &dstLocation,
        0, 0, 0,
        &srcLocation,
        nullptr
    );
}

void Model::copyBuffer(
    const Microsoft::WRL::ComPtr<ID3D12Resource> &srcBuffer,
    const Microsoft::WRL::ComPtr<ID3D12Resource> &dstBuffer
) const
{
    m_copyCommandList->CopyResource(
        dstBuffer.Get(),
        srcBuffer.Get()
    );
}

void Model::executeCopy()
{
    HRESULT hr = m_copyCommandList->Close();
    if (FAILED(hr))
    {
        std::cerr << "Failed to close copy command list." << std::endl;
        return;
    }

    std::array<ID3D12CommandList*, 1> commandLists = { m_copyCommandList.Get() };
    m_copyCommandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

    m_copyFenceValue++;
    hr = m_copyCommandQueue->Signal(m_copyFence.Get(), m_copyFenceValue);
    if (FAILED(hr))
    {
        std::cerr << "Failed to signal copy command queue." << std::endl;
        return;
    }

    if (m_copyFence->GetCompletedValue() < m_copyFenceValue)
    {
        hr = m_copyFence->SetEventOnCompletion(m_copyFenceValue, m_copyFenceEvent);
        if (FAILED(hr))
        {
            std::cerr << "Failed to set event on copy fence completion." << std::endl;
            return;
        }
        WaitForSingleObject(m_copyFenceEvent, INFINITE);
    }

    hr = m_copyCommandAllocator->Reset();
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset copy command allocator." << std::endl;
        return;
    }

    hr = m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset copy command list." << std::endl;
        return;
    }

    m_waitForCopyResources.clear();
}

void Model::barrier(
    const Microsoft::WRL::ComPtr<ID3D12Resource> &resource,
    D3D12_RESOURCE_STATES beforeState,
    D3D12_RESOURCE_STATES afterState
)
{
    m_barriers.push_back(
        D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource = resource.Get(),
                .Subresource = 0,
                .StateBefore = beforeState,
                .StateAfter = afterState
            }
        }
    );
}
